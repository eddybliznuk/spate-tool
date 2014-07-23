/*
    SPATE - HTTP performance test tool

    Copyright (C) 2014  Edward Blizniuk (known also as Ed Blake)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "worker.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include "dispatcher.h"
#include "utils.h"
#include "log.h"
#include "request.h"

/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/

Socket_t* get_socket_handler(int sock_fd)
{
	Socket_t* sh = (Socket_t*)malloc(sizeof(Socket_t));

	if(sh == NULL)
		return NULL;

	memset(sh, 0, sizeof(Socket_t));

	sh->sock_fd				= sock_fd;
	sh->first_read 			= 1;
	sh->parser.reply_len	= UINT32_MAX;

	return sh;
}

static Error_t create_socket_and_connect (Worker_t* w)
{
	int sock_fd;

    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	LOG_ERR("Can't create socket. Error %d : %s. Connection terminated.\n", errno, strerror(errno))
        return ERR_CANT_CREATE_SOCKET;
    }

	long arg = fcntl(sock_fd, F_GETFL, NULL);
	arg |= O_NONBLOCK;
	fcntl(sock_fd, F_SETFL, arg);

	int on=1;
	setsockopt (sock_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

	Tuple_t* t = w->cur_tuple->t;

	/* If user specifies remote target in the command line the local address is not specified,
	   so we don't bind and let Kernel to choose the local interface automatically
	 */
	if (t->local_addr.sin_family != 0)
	{
		if (bind(sock_fd, (struct sockaddr *)&t->local_addr, sizeof(t->local_addr)) != 0)
		{
			LOG_ERR("Can't bind local socket. Error %d : %s. Connection terminated.\n", errno, strerror(errno));
			return CANT_CONNECT_SOCKET;
		}
	}

    if (connect(sock_fd, (struct sockaddr *)&t->remote_addr, sizeof(t->remote_addr)) != 0)
    {
    	if (errno != EINPROGRESS)
    	{
    		LOG_ERR("Can't connect to remote socket. Error %d : %s. Connection terminated.\n", errno, strerror(errno));
    		return CANT_CONNECT_SOCKET;
    	}
    }

	// Round Robin on tuple list
	if(w->cur_tuple->next == NULL)
		w->cur_tuple = w->tuple_list;
	else
		w->cur_tuple = w->cur_tuple->next;

#ifdef _DEBUG
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(sock_fd, (struct sockaddr *)&sin, &len);
	char addr_str[STR_BUF_LEN];
	utils_sockaddr_to_string((struct sockaddr*)&sin, addr_str, STR_BUF_LEN);
	LOG_DEBUG("WORKER_%d : Created socket [%d] on local ip [%s]\n", w->worker_id, sock_fd, addr_str);
#endif

	Socket_t* sh;

	if((sh = get_socket_handler(sock_fd)) == NULL)
	{
		LOG_ERR("WORKER_%d : Out of memory\n", w->worker_id);
		return ERR_MEMORY_ALLOC;
	}

	sh->write_buf = sh->req_buf	= t->req_buf;
	sh->write_buf_size = sh->req_buf_size = strlen(t->req_buf);

	struct epoll_event event;

	event.data.ptr	= (void*)sh;
    event.events	= EPOLLOUT;

    if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) != 0)
    {
    	LOG_ERR("Can't add socket to epoll. Error %d : %s. Connection terminated.\n", errno, strerror(errno))
    	return CANT_ADD_TO_EPOLL;
    }

    ++w->stat.c[SOCK_OPEN];
    ++w->sockets;

    return ERR_OK;
}


static void close_sock(Worker_t* w, struct epoll_event* event)
{
	Socket_t* sh = (Socket_t*)event->data.ptr;

#ifdef _DEBUG
	LOG_DEBUG("WORKER_%d : Socket [%d] closed\n", w->worker_id, sh->sock_fd);
#endif

	event->events = 0;

	epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, sh->sock_fd, event);

	close (sh->sock_fd);

	if (event->data.ptr != NULL)
		free(event->data.ptr);

	event->data.ptr = NULL;

	if (--w->sockets == 0 && w->state == WORKER_STATE_CLEANUP)
		/* No more sockets open - stop the main loop now */
		w->stop_loop = 1;
}

static void process_wait_list(Worker_t* w)
{
	while(w->wait_list_head != NULL &&
		  (utils_timeval_diff(&w->now, &w->wait_list_head->next_request_time) > 0))
	{
		struct epoll_event event;
		Socket_t* sh = w->wait_list_head;

		event.data.ptr = (void*)sh;
		event.events = EPOLLOUT;
		if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, sh->sock_fd, &event) != 0)
		{
			w->stat.c[EPOLL_ERR]++;
			close_sock (w, &event);
		}

		w->wait_list_head = sh->next;
	}
}

static void add_to_wait_list(Worker_t* w, Socket_t* sh)
{
	if (w->wait_list_head == NULL)
	{
		memcpy(&sh->next_request_time, &w->now, sizeof(sh->next_request_time));
		utils_timeval_inc (&sh->next_request_time, w->delay);

		w->wait_list_head = w->wait_list_tail = sh;
		sh->next = sh->prev = NULL;

		return;
	}
	w->wait_list_tail->next = sh;
	sh->next 				= NULL;
	sh->prev 				= w->wait_list_tail;
	w->wait_list_tail 		= sh;
}

static void send_request(Worker_t* w, struct epoll_event *event)
{
	Socket_t* 	sh = (Socket_t*)event->data.ptr;
	int 		written_size;

	if (w->state == WORKER_STATE_CLEANUP)
	{
		/* In cleanup state we don't send new requests - just close the socket */
		close_sock (w, event);
		return;
	}

	for(int i=0; i<LOOP_GUARD; i++)
	{

		written_size = write(sh->sock_fd, sh->write_buf, (sh->write_buf_size < MAX_WRITE_SIZE)?sh->write_buf_size:MAX_WRITE_SIZE );

		if (written_size == -1)
		{
			if (errno != EAGAIN)
			{
		    	LOG_ERR("Can't write to socket [%d]. Error %d : %s. Closing the socket.\n", sh->sock_fd, errno, strerror(errno))
				++w->stat.c[SOCK_ERR];
				close_sock (w, event);
			}
			break;
		}
		else
		{
			/* printf ("Sock %d : Wrote %d\n", sh->sock_fd, written_size); */

			w->stat.c[BYTES_SENT] 	+= written_size;
			sh->write_buf_size		-= written_size;
			sh->write_buf			+= written_size;

			if (sh->write_buf_size <= 0)
			{
				/* Request is sent - wait for reply */

				w->stat.c[REQ_SENT]++;
				++sh->requests;

				sh->write_buf 		= sh->req_buf;
				sh->write_buf_size	= sh->req_buf_size;
			    sh->first_read		= 1;

				event->events = EPOLLIN;
				if (epoll_ctl(w->epoll_fd, EPOLL_CTL_MOD, sh->sock_fd, event) != 0)
				{
			    	LOG_ERR("Can't modify epoll event for socket [%d]. Closing the socket.\n", w->epoll_fd);
					w->stat.c[EPOLL_ERR]++;
					close_sock (w, event);
				}
			}

			break;
		}
	}
}


static void receive_reply(Worker_t* w, struct epoll_event *event)
{
	Socket_t* 	sh = (Socket_t*)event->data.ptr;
	int  		read_size;

	for(int i=0; i<LOOP_GUARD; i++)
	{
        if ((read_size = read(sh->sock_fd, w->read_buf, w->read_buf_size)) == -1)
        {
			if (errno != EAGAIN)
			{
		    	LOG_ERR("Can't read from socket [%d]. Error %d : %s. Closing the socket.\n", sh->sock_fd, errno, strerror(errno))
				++w->stat.c[SOCK_ERR];
				close_sock (w, event);
			}
			break;
	    }
        else if (read_size == 0)
        {
        	close_sock (w, event);
        }
        else
        {
        	/* printf("Sock %d : read %d\n", sh->sock_fd, read_size); */

        	if (sh->parser.reply_len == UINT32_MAX || w->parse_every_resp)
        	{
        		parser_parse_http(&sh->parser, w->read_buf, read_size, sh->first_read);
    			sh->first_read = 0;
        	}

			w->stat.c[BYTES_RCVD]	+= read_size;
			sh->resp_rcvd_size 		+= read_size;

			/* TODO If reply is junk, no size determined by parser => cancel all */
			if (sh->resp_rcvd_size >= sh->parser.reply_len)
			{
				++w->stat.c[RESP_RCVD];
				sh->resp_rcvd_size = 0;

				if (w->state == WORKER_STATE_CLEANUP ||
				   (w->req_per_conn > 0 && sh->requests >= w->req_per_conn))
				{
					close_sock (w, event);
					break;
				}

				if (w->delay > 0)
				{
					event->events = 0;

					if (epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, sh->sock_fd, event) != 0)
					{
						w->stat.c[EPOLL_ERR]++;
						close_sock (w, event);
					}

					add_to_wait_list(w, sh);
				}
				else
				{
					/* The reply is received - send new request on the same connection */
					event->events = EPOLLOUT;
					if (epoll_ctl(w->epoll_fd, EPOLL_CTL_MOD, sh->sock_fd, event) != 0)
					{
						++w->stat.c[EPOLL_ERR];
						close_sock (w, event);
					}
				}

				break;
			}
        }
	}

	return;
}

Error_t add_sockets(Worker_t* w)
{
	while (w->sockets < w->sock_max_count)
	{
		Error_t rc;
		if ((rc=create_socket_and_connect(w)) != ERR_OK)
			return rc;
	}

	return ERR_OK;
}

Error_t do_start_step(Worker_t* w)
{

	static EggBasket_t eb;

	if (w->step == 1)
	{
		utils_eggbasket_init(&eb, w->conn_count, w->start_steps);
		w->sock_max_count = 0;
	}

	w->sock_max_count += utils_eggbasket_get(&eb);

	gettimeofday(&w->last_step_time, NULL);

	add_sockets(w);

#ifdef _TRACE
	printf ("WORKER_%d : %d sockets open on step [%d]\n", w->worker_id, w->sockets, w->step);
#endif

	++w->step;

	if(w->step > w->start_steps)
		/* Step-by-step period finished, start to calculate averages in statistics */
		w->full_load = 1;

	return ERR_OK;
}


static void free_tuple_list(TupleList_t* tl)
{
	while (tl != NULL)
	{
		TupleList_t* next = tl->next;
		free (tl);
		tl = next;
	}
}

/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/


Error_t worker_init (Worker_t* w, uint16_t id, uint32_t conn_count, uint64_t req_count, unsigned long affinity)
{
	memset(w, 0, sizeof(Worker_t));

	w->worker_id 			= id;
	w->conn_count 			= conn_count;
	w->req_count 			= req_count;
	w->epoll_fd				= -1;
	w->sockets				= 0;
	w->state 				= WORKER_STATE_IDLE;
	w->step_state			= STEP_STATE_INIT;
	w->step					= 1;
	w->sock_max_count		= 0;
	w->tuple_list			= NULL;
	w->affinity				= affinity;

	/* We keep copies of global parameters in order to optimize cache usage */

	w->req_per_conn			= g_params.req_per_conn;
	w->delay				= g_params.delay;
	w->parse_every_resp		= g_params.parse_every_resp;
	w->start_steps			= g_params.start_steps;
	w->step_time			= g_params.step_time;
	w->epoll_max_events		= g_params.epoll_max_events;
	w->epoll_timeout		= g_params.epoll_timeout;
	w->cleanup_timeout 		= g_params.worker_cleanup_timeout;

	w->read_buf 			= (char*)malloc(g_params.resp_buf_size);
	w->read_buf_size 		= g_params.resp_buf_size;

	if(w->read_buf == NULL)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for read buffer\n", w->worker_id);
		return ERR_MEMORY_ALLOC;
	}

#ifdef _TRACE
	printf ("WORKER_%d : Initialized with params : conn_count=%d, request_count=%lu, affinity=%lx \n",
			w->worker_id, w->conn_count, w->req_count, w->affinity );
#endif

	return ERR_OK;
}

Error_t worker_add_tuple (Worker_t* w, Tuple_t* t)
{
	Error_t rc;

	if((rc = request_create_request(&t->req_buf, &t->url)) != ERR_OK)
	{
		LOG_ERR("WORKER_%d can't create request\n", w->worker_id);
		return rc;
	}

	TupleList_t* tl = malloc(sizeof(TupleList_t));

	if(tl == NULL)
		return ERR_MEMORY_ALLOC;

	tl->t 			= t;
	tl->next 		= w->tuple_list;
	w->tuple_list 	= tl;
	w->cur_tuple 	= w->tuple_list;

#ifdef _TRACE
	char local[STR_BUF_LEN];
	char remote[STR_BUF_LEN];
	local[0] = remote[0] = 0;
	utils_sockaddr_to_string((struct sockaddr*)&t->local_addr, local, STR_BUF_LEN);
	utils_sockaddr_to_string((struct sockaddr*)&t->remote_addr, remote, STR_BUF_LEN);
	printf("WORKER_%d : Adding tuple: local [%s], remote [%s]\n", w->worker_id, local, remote );
#endif

	return ERR_OK;
}


void*  worker_do_work (void* param)
{
	Worker_t* 	w = (Worker_t*)param;
	Error_t		rc;

	if(w->affinity > 0)
	{
		if(pthread_setaffinity_np(w->thread, sizeof(w->affinity), &w->affinity) != 0)
		{
			LOG_ERR("WORKER_%d : Can't set affinity [%lx].\n", w->worker_id, w->affinity);
			dispatcher_on_finish();
			return NULL;
		}
#ifdef _TRACE
		printf ("WORKER_%d : Setting CPU affinity mask [%lx].\n", w->worker_id,  w->affinity);
#endif
	}

	if ((w->epoll_fd = epoll_create1(0)) == -1)
	{
		LOG_ERR("WORKER_%d : Can't create epoll.\n", w->worker_id);
		dispatcher_on_finish();
		return NULL;
	}

	if ((rc = do_start_step(w)) != ERR_OK)
	{
		dispatcher_on_finish();
		return NULL;
	}

	struct epoll_event*	events = (struct epoll_event*) malloc(sizeof(struct epoll_event) * w->epoll_max_events);

	if (events == NULL)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for [%d] epoll events.\n", w->worker_id, w->epoll_max_events);
		dispatcher_on_finish();
		return NULL;
	}

	w->state 		= WORKER_STATE_IN_PROGRESS;
	w->stop_loop	= 0;

	while(!w->stop_loop)
	{
		int i, n;

		gettimeofday(&w->now, NULL);
		process_wait_list(w);

		if(w->state == WORKER_STATE_CLEANUP &&
		   utils_timeval_diff(&w->now, &w->cleanup_start) > w->cleanup_timeout)
		{
#ifdef _TRACE
			printf ("WORKER_%d : Finished on cleanup timeout with %d open sockets\n", w->worker_id, w->sockets);
#endif
			break;
		}

		/* Processing step-by-step traffic increase */
		if(w->step <= w->start_steps && utils_timeval_diff(&w->now, &w->last_step_time) >= w->step_time)
		{
			if ((rc = do_start_step(w)) != ERR_OK)
			{
				dispatcher_on_finish();
				return NULL;
			}
		}

		/*  Processing ReqPerConnection and/or open new sockets to replace closed ones */
		if(w->sockets < w->sock_max_count && w->state == WORKER_STATE_IN_PROGRESS)
			add_sockets(w);

		stat_refresh_worker_stat(&w->stat, w->worker_id, w->full_load);

		n = epoll_wait(w->epoll_fd, events, 1000 /*w->epoll_max_events*/, w->epoll_timeout);

		/* Process requests and responses in a separate loops for more efficient CPU cache usage */

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLIN)
			{
				receive_reply(w, &events[i]);
			}
		}

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLOUT)
			{
				send_request(w, &events[i]);

				if (w->stat.c[REQ_SENT] >= w->req_count &&
					w->state == WORKER_STATE_IN_PROGRESS)
				{
#ifdef _TRACE
					printf ("WORKER_%d : All requests are sent. Entering cleanup phase\n", w->worker_id);
#endif
					w->state = WORKER_STATE_CLEANUP;
					gettimeofday(&w->cleanup_start, NULL);
				}
			}
			else if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
			{
				if (events[i].events & EPOLLERR)
					w->stat.c[SOCK_ERR]++;

				if (events[i].events & EPOLLHUP)
					w->stat.c[SOCK_ERR]++;

				close_sock (w, &events[i]);
				continue;
			}

		} /* for */

	} /* while */

	free(events);
	free_tuple_list (w->tuple_list);

	stat_refresh_worker_stat(&w->stat, w->worker_id, w->full_load);
	dispatcher_on_finish();

	return NULL;
}

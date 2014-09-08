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

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#include "dispatcher.h"
#include "utils.h"
#include "log.h"
#include "request.h"
#include "response.h"

static Error_t create_socket_and_connect (Worker_t* w);


/****************************************************************
 *
 *                   Common Private Functions
 *
 ****************************************************************/

static Socket_t* get_socket_handler()
{
	Socket_t* sh = (Socket_t*)malloc(sizeof(Socket_t));

	if(sh == NULL)
		return NULL;

	memset(sh, 0, sizeof(Socket_t));

	sh->first_read 				= 1;
	sh->parser.msg_len			= UINT32_MAX;
	sh->rcvd_size				= 0;

	return sh;
}

void free_tuple_list(TupleList_t* tl)
{
	while (tl != NULL)
	{
		TupleList_t* next = tl->next;
		free (tl);
		tl = next;
	}
}

static void add_to_wait_list(Worker_t* w, Socket_t* sh)
{
	if (w->wait_list_head == NULL)
	{
		memcpy(&sh->next_action_time, &w->now, sizeof(sh->next_action_time));
		utils_timeval_inc (&sh->next_action_time, w->delay);

		w->wait_list_head = w->wait_list_tail = sh;
		sh->next = sh->prev = NULL;

		return;
	}

	w->wait_list_tail->next = sh;
	sh->next 				= NULL;
	sh->prev 				= w->wait_list_tail;
	w->wait_list_tail 		= sh;
}

static Error_t add_sockets(Worker_t* w)
{
	while (w->sockets < w->sock_max_count)
	{
		Error_t rc;

		if ((rc=create_socket_and_connect(w)) != ERR_OK)
			return rc;
	}

	return ERR_OK;
}

static Error_t do_start_step(Worker_t* w)
{
	if (w->step == 1)
	{
		utils_eggbasket_init(&w->step_eb, w->conn_count, w->start_steps);
		w->sock_max_count = 0;
	}

	w->sock_max_count += utils_eggbasket_get(&w->step_eb);

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

/****************************************************************
 *
 *        Private Functions per Architecture (Spool/Epoll)
 *
 ****************************************************************/


#ifdef _SPOOL

static void add_to_current_wbd_list(Worker_t* w, Socket_t* sh)
{
	sh->wbd.next = w->wbd_head[w->cur_wbd_list];
	w->wbd_head[w->cur_wbd_list] = &sh->wbd;

	if(++w->wbd_list_size[w->cur_wbd_list] >= w->sock_list_size)
	{
		if(++w->cur_wbd_list > SOCK_LIST_COUNT)
		{
			/* We assume here that the 'older' lists shrank once sockets were closed
			   so we start adding to the first list again  */
			w->cur_wbd_list = 0;
		}
	}
}

static void close_spool_sock(Worker_t* w, Socket_t* sh, uint32_t del_from_hash)
{
	int res = 0;

	if (sh != NULL)
	{
		struct spool_sock	spool_sock;

		LOG_DEBUG("WORKER_%d : Closing socket [%ld]\n", w->worker_id, sh->sock_fd);

		if (sh->flags & SOCK_FLAG_FOREIGN)
		{
			listener_close_foreign_sock(w->listener, sh->sock_fd);
		}
		else
		{
			spool_sock.sock_h = sh->sock_fd;

			res = ioctl(w->spool_fd, SPOOL_IO_CLOSESOCK, &spool_sock);
			if(res < 0)
			{
				LOG_ERR("WORKER_%d  : Spool ioctl failed on socket close with error [%d]\n",w->worker_id, res);
			}
		}

		++w->stat.c[SOCK_CLOSE];
		--w->sockets;

		if (w->state == WORKER_STATE_CLEANUP && w->sockets == 0)
				w->stop_loop = 1;

		if(del_from_hash == 1)
			utils_hash_del(&w->sock_hash, sh);

		free(sh);
	}
}

static Error_t create_socket_and_connect (Worker_t* w)
{
	struct spool_sock 	ss;
	Tuple_t* 			t = w->cur_tuple->t;
	int 				res;

	if (t->local_addr.sin_family != 0)
		memcpy(&ss.local, &t->local_addr, sizeof(ss.local));
	else
		ss.local.sin_family = 0;

	memcpy(&ss.remote, &t->remote_addr, sizeof(ss.remote));

	ss.flags = SPOOL_FLAG_CLIENT_SOCK;

	res = ioctl(w->spool_fd, SPOOL_IO_ADDSOCK, &ss);
	if(res < 0)
	{
		LOG_ERR("WORKER_%d : Spool ioctl failed with error [%d]\n", w->worker_id, res);
		return CANT_ADD_TO_EPOLL;
	}

	if(w->cur_tuple->next == NULL)
		w->cur_tuple = w->tuple_list;
	else
		w->cur_tuple = w->cur_tuple->next;

	Socket_t* sh;

	if((sh = get_socket_handler()) == NULL)
	{
		LOG_ERR("WORKER_%d : Out of memory\n", w->worker_id);
		return ERR_MEMORY_ALLOC;
	}

	add_to_current_wbd_list(w, sh);

	++w->req_processed;

	sh->wbd.sock_h 		= ss.sock_h;
	sh->wbd.buff 		= t->req_buff;
	sh->wbd.size 		= strlen(t->req_buff);
	sh->wbd.offset		= 0;
	sh->wbd.status		= SPOOL_STAT_OK;
	sh->wbd.private		= (void*)sh;

	sh->rbd.sock_h 		= ss.sock_h;
	sh->read_buff 		= sh->rbd.buff 	= w->read_buff;
	sh->read_buff_size	= sh->rbd.size 	= w->read_buff_size;
	sh->rbd.offset 		= 0;
	sh->rbd.status 		= SPOOL_STAT_DISABLED;
	sh->rbd.private		 = (void*)sh;

	sh->sock_fd 		= ss.sock_h;

    ++w->stat.c[SOCK_OPEN];
    ++w->sockets;

    return ERR_OK;
}

static Error_t accept_spool(Worker_t* w)
{
	struct spool_sock* ss;

	while ((ss = (struct spool_sock*)fifo_ring_get(&w->accept_q)) != NULL)
	{
		struct spool_sock* cur_ss = ss;

		for(int i=0; i<MAX_ACCEPTED_SOCKS; i++)
		{
			Socket_t* 	sh;

			if(cur_ss->sock_h == 0)
				break;

			++w->stat.c[SOCK_OPEN];

			if((sh = get_socket_handler()) == NULL)
			{
				LOG_ERR("WORKER_%d : Out of memory\n", w->worker_id);
				return ERR_MEMORY_ALLOC;
			}

			sh->flags 			|= SOCK_FLAG_FOREIGN;

			sh->wbd.sock_h 		= cur_ss->sock_h;
			sh->wbd.buff		= w->write_buff;
			sh->wbd.size		= w->write_buff_size;
			sh->wbd.offset 		= 0;
			sh->wbd.status 		= SPOOL_STAT_DISABLED;
			sh->wbd.private 	= (void*)sh;

			sh->rbd.sock_h		= cur_ss->sock_h;
			sh->read_buff 		= sh->rbd.buff 	= w->read_buff;
			sh->read_buff_size	= sh->rbd.size 	= w->read_buff_size;
			sh->rbd.offset 		= 0;
			sh->rbd.status 		= SPOOL_STAT_OK;
			sh->rbd.private 	= (void*)sh;

			sh->sock_fd 		= cur_ss->sock_h;
			sh->first_read		= 1;
			sh->rcvd_size		= 0;

			utils_hash_add(&w->sock_hash, sh);

			/* Add to read socket list */
			sh->rbd.next = w->rbd_head[w->cur_rbd_list];
			w->rbd_head[w->cur_rbd_list] = &sh->rbd;

			if(++w->rbd_list_size[w->cur_rbd_list] >= w->sock_list_size)
			{
				if(++w->cur_rbd_list > SOCK_LIST_COUNT)
				{
					/* We assume here that the 'older' lists shrank once sockets were closed
					   so we start adding to the first list again  */
					w->cur_rbd_list = 0;
				}
			}


			++cur_ss;
		}

		free(ss);
	}

	return ERR_OK;
}


static void process_spool_wait_list(Worker_t* w)
{
	while(w->wait_list_head != NULL &&
		  (utils_timeval_diff(&w->now, &w->wait_list_head->next_action_time) > 0))
	{
		Socket_t* sh = w->wait_list_head;

		add_to_current_wbd_list (w, sh);
		w->wait_list_head = sh->next;
	}
}


static void read_spool(Worker_t* w, uint32_t list_id)
{
	int res;
	struct spool_sbd** cur_rbd;

	res = read(w->spool_fd, w->rbd_head[list_id], 0);

	if(res < 0 )
	{
		LOG_ERR("WORKER_%d : Spool read error [%d].\n", w->worker_id, res);
		return;
	}

	cur_rbd = &w->rbd_head[list_id];

	while (*cur_rbd != NULL)
	{
		Socket_t* sh = (Socket_t*)((*cur_rbd)->private);

		if ((*cur_rbd)->status == SPOOL_STAT_OK)
		{

			if (sh->parser.msg_len == UINT32_MAX || w->parse_every_http)
			{
				parser_parse_http(&sh->parser, (*cur_rbd)->buff, (*cur_rbd)->offset, sh->first_read);
				sh->first_read = 0;
			}

			w->stat.c[BYTES_RCVD]	+= (*cur_rbd)->offset;
			sh->rcvd_size			+= (*cur_rbd)->offset;

			(*cur_rbd)->buff 	= sh->read_buff;
			(*cur_rbd)->size 	= sh->read_buff_size;
			(*cur_rbd)->offset 	= 0;

			if (sh->rcvd_size >= sh->parser.msg_len)
			{
				++w->stat.c[MSG_RCVD];

				sh->rcvd_size = 0;
				(*cur_rbd)->status = SPOOL_STAT_DISABLED;

				if ((w->req_count != 0) && (++w->req_processed > w->req_count))
				{
					gettimeofday(&w->cleanup_start, NULL);
					w->state = WORKER_STATE_CLEANUP;
				}

				if(w->state == WORKER_STATE_IN_PROGRESS)
				{
					sh->wbd.status = SPOOL_STAT_OK;
					sh->wbd.offset = 0;

					if (w->delay > 0)
					{
						add_to_wait_list(w, sh);
					}
					else
					{
						// Add to write list
						struct spool_sbd* tmp = &sh->wbd;
						tmp->next = w->wbd_head[list_id];
						w->wbd_head[list_id] = tmp;
						++w->wbd_list_size[list_id];
					}
				}
				else if(w->state == WORKER_STATE_CLEANUP)
				{
					close_spool_sock(w, sh, 1);
				}

				goto remove_and_continue;
			}
			else
			{
				goto keep_and_continue;
			}
		}
		else if ((*cur_rbd)->status == SPOOL_STAT_CONN_CLOSED)
		{
			/* Socket was closed by the peer */
			close_spool_sock(w, sh, 1);
			goto remove_and_continue;
		}
		else if ((*cur_rbd)->status == SPOOL_STAT_NOT_READY)
		{
			goto keep_and_continue;
		}
		else
		{
			w->stat.c[SOCK_ERR]++;
			close_spool_sock(w, sh, 1);
			goto remove_and_continue;
		}

keep_and_continue:
		if (*cur_rbd)
			cur_rbd = &((*cur_rbd)->next);
		continue;

remove_and_continue:
		++w->rbd_list_size[list_id];
		*cur_rbd = (*cur_rbd)->next;
	}
}

static void write_spool(Worker_t* w, uint32_t list_id)
{
	int res;
	struct spool_sbd** cur_wbd;

	process_spool_wait_list(w);

	res = write(w->spool_fd, w->wbd_head[list_id], 0);

	if(res < 0 )
	{
		LOG_ERR("WORKER_%d : Spool write error [%d].\n", w->worker_id, res);
		return;
	}

	cur_wbd = &w->wbd_head[list_id];

	while (*cur_wbd != NULL)
	{
		Socket_t* sh = (Socket_t*)((*cur_wbd)->private);

		if((*cur_wbd)->status == SPOOL_STAT_OK)
		{

			w->stat.c[BYTES_SENT] += (*cur_wbd)->offset;

			if((*cur_wbd)->offset == (*cur_wbd)->size)
			{
				w->stat.c[MSG_SENT]++;
				(*cur_wbd)->status = SPOOL_STAT_DISABLED;

				// TODO Add to the end of read list instead of the head
				struct spool_sbd* tmp = &sh->rbd;
				tmp->next = w->rbd_head[list_id];
				w->rbd_head[list_id] = tmp;

				tmp->status 		= SPOOL_STAT_OK;
				tmp->offset			= 0;
				sh->first_read		= 1;

				// Remove from write list
				*cur_wbd = (*cur_wbd)->next;

				continue;
			}
		}
		else if ((*cur_wbd)->status != SPOOL_STAT_NOT_READY)
		{
			w->stat.c[SOCK_ERR]++;

			close_spool_sock(w, sh, 1);
			*cur_wbd = (*cur_wbd)->next;
			continue;
		}

		if (*cur_wbd)
			cur_wbd = &((*cur_wbd)->next);

	} /* while */
}


static Error_t do_server_loop(Worker_t* w)
{
	Error_t		rc;
	uint32_t	list_id = 0;

	w->spool_fd = open("/dev/spool", O_RDWR);

	if (w->spool_fd < 0)
	{
		LOG_ERR("WORKER_%d : Can't open spool file. Error %d : %s.\n", w->worker_id, errno, strerror(errno));
		return ERR_CANT_CREATE_EPOLL;
	}

	while(!w->stop_loop)
	{
		gettimeofday(&w->now, NULL);

		if((rc = accept_spool(w)) != ERR_OK)
		{
			LOG_ERR("WORKER_%d : Accept failed.\n", w->worker_id);
			return rc;
		}

		read_spool(w, list_id);
		write_spool(w, list_id);

		if(++list_id > w->cur_rbd_list)
			list_id = 0;

		//usleep(10);

		stat_refresh_worker_stat(&w->stat, w->worker_id, 1);
	}

	close(w->spool_fd);

	return ERR_OK;
}

static Error_t do_client_loop(Worker_t* w)
{
	Error_t		rc;
	uint32_t	list_id = 0;

	w->spool_fd = open("/dev/spool", O_RDWR);

	if (w->spool_fd < 0)
	{
		LOG_ERR("WORKER_%d : Can't open spool file. Error %d : %s.\n", w->worker_id, errno, strerror(errno));
		return ERR_CANT_CREATE_EPOLL;
	}

	if ((rc = do_start_step(w)) != ERR_OK)
		return rc;

	while(!w->stop_loop)
	{
		gettimeofday(&w->now, NULL);

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
				return rc;
		}

		write_spool(w, list_id);
		read_spool(w, list_id);

		if(++list_id > w->cur_wbd_list)
			list_id = 0;

		sleep(0);

		stat_refresh_worker_stat(&w->stat, w->worker_id, w->full_load);

	} /* while */

	close(w->spool_fd);

	return ERR_OK;
}

#else

static void close_epoll_sock(Worker_t* w, struct epoll_event* event)
{
	Socket_t* sh = (Socket_t*)event->data.ptr;

	LOG_DEBUG("WORKER_%d : Socket [%d] closed %lx\n", w->worker_id, sh->sock_fd, sh);

	event->events 	= 0;
	event->data.ptr = NULL;

	if (sh != NULL)
	{
		epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, sh->sock_fd, event);
		close (sh->sock_fd);
		free(sh);
	}

	++w->stat.c[SOCK_CLOSE];
	--w->sockets;

	if (w->state == WORKER_STATE_CLEANUP && w->sockets == 0)
		/* No more sockets open - stop the main loop now */
		w->stop_loop = 1;
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

	sh->write_buff 		= sh->cur_write_buff		= t->req_buff;
	sh->write_buff_size = sh->cur_write_buff_size 	= strlen(t->req_buff);
	sh->sock_fd 		= sock_fd;

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

static Error_t accept_epoll(Worker_t* w)
{
	int 	sock_fd;
	int*	p;

	while ((p = (int*)fifo_ring_get(&w->accept_q)) != 0)
	{
		sock_fd = *p;
		free(p);

		long arg = fcntl(sock_fd, F_GETFL, NULL);
		arg |= O_NONBLOCK;
		fcntl(sock_fd, F_SETFL, arg);

		int on=1;
		setsockopt (sock_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

		Socket_t* sh;

		if((sh = get_socket_handler()) == NULL)
		{
			LOG_ERR("WORKER_%d : Out of memory\n", w->worker_id);
			return ERR_MEMORY_ALLOC;
		}

		sh->sock_fd			= sock_fd;
		sh->read_buff		= w->read_buff;
		sh->read_buff_size	= w->read_buff_size;
		sh->write_buff 		= w->write_buff;
		sh->write_buff_size = w->write_buff_size;

		struct epoll_event event;

		event.data.ptr	= (void*)sh;
	    event.events	= EPOLLIN;

	    if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, sock_fd, &event) != 0)
	    {
	    	LOG_ERR("Can't add socket to epoll. Error %d : %s. Connection terminated.\n", errno, strerror(errno))
	    	return CANT_ADD_TO_EPOLL;
	    }

	    ++w->stat.c[SOCK_OPEN];
	    ++w->sockets;

	}

	return ERR_OK;
}

static void process_epoll_wait_list(Worker_t* w)
{
	while(w->wait_list_head != NULL &&
		  (utils_timeval_diff(&w->now, &w->wait_list_head->next_action_time) > 0))
	{
		struct epoll_event event;
		Socket_t* sh = w->wait_list_head;

		event.data.ptr = (void*)sh;
		event.events = EPOLLOUT;
		if (epoll_ctl(w->epoll_fd, EPOLL_CTL_ADD, sh->sock_fd, &event) != 0)
		{
			w->stat.c[EPOLL_ERR]++;
			close_epoll_sock (w, &event);
		}

		w->wait_list_head = sh->next;
	}
}


static void write_epoll(Worker_t* w, struct epoll_event *event)
{
	Socket_t* 	sh = (Socket_t*)event->data.ptr;
	int 		written_size;

	if (w->state == WORKER_STATE_CLEANUP)
	{
		/* In cleanup state we don't send new requests - just close the socket */
		close_epoll_sock (w, event);
		return;
	}

	for(int i=0; i<LOOP_GUARD; i++)
	{
		written_size = write(sh->sock_fd, sh->cur_write_buff, sh->cur_write_buff_size);

		if (written_size < 0)
		{
			if (errno != EAGAIN)
			{
		    	LOG_ERR("Can't write to socket [%ld]. Error %d : %s. Closing the socket.\n", sh->sock_fd, errno, strerror(errno))
				++w->stat.c[SOCK_ERR];
				close_epoll_sock (w, event);
			}
			break;
		}
		else
		{
			w->stat.c[BYTES_SENT] 		+= written_size;
			sh->cur_write_buff_size		-= written_size;
			sh->cur_write_buff			+= written_size;

			if (sh->cur_write_buff_size <= 0)
			{
				w->stat.c[MSG_SENT]++;
				++sh->requests;

			    sh->first_read		= 1;

				event->events = EPOLLIN;
				if (epoll_ctl(w->epoll_fd, EPOLL_CTL_MOD, sh->sock_fd, event) != 0)
				{
			    	LOG_ERR("Can't modify epoll event for socket [%d]. Closing the socket.\n", w->epoll_fd);
					w->stat.c[EPOLL_ERR]++;
					close_epoll_sock (w, event);
				}
			}

			break;
		}
	}
}

static void read_epoll(Worker_t* w, struct epoll_event *event)
{
	Socket_t* 	sh = (Socket_t*)event->data.ptr;
	int  		read_size;

	for(int i=0; i<LOOP_GUARD; i++)
	{
		read_size = read(sh->sock_fd, w->read_buff, w->read_buff_size);

    	//printf("Sock %d : read %d\n", sh->sock_fd, read_size);

        if (read_size <0)
        {
			if (errno != EAGAIN)
			{
		    	LOG_ERR("Can't read from socket [%ld]. Error %d : %s. Closing the socket.\n", sh->sock_fd, errno, strerror(errno))
				++w->stat.c[SOCK_ERR];
				close_epoll_sock (w, event);
			}
			break;
	    }
        else if (read_size == 0)
        {
        	++w->stat.c[SOCK_HUP];
        	close_epoll_sock(w, event);
        	break;
        }
        else
        {
        	if (sh->parser.msg_len == UINT32_MAX || w->parse_every_http)
        	{
        		parser_parse_http(&sh->parser, w->read_buff, read_size, sh->first_read);
    			sh->first_read = 0;
        	}

			w->stat.c[BYTES_RCVD]	+= read_size;
			sh->rcvd_size 			+= read_size;

			/* TODO If reply is junk, no size determined by parser => cancel all */
			if (sh->rcvd_size >= sh->parser.msg_len)
			{
				++w->stat.c[MSG_RCVD];
				sh->rcvd_size = 0;

				if (w->state == WORKER_STATE_CLEANUP ||
				   (w->req_per_conn > 0 && sh->requests >= w->req_per_conn))
				{
					close_epoll_sock (w, event);
					break;
				}

				sh->cur_write_buff 			= sh->write_buff;
				sh->cur_write_buff_size  	= sh->write_buff_size;

				if (w->delay > 0)
				{
					event->events = 0;

					if (epoll_ctl(w->epoll_fd, EPOLL_CTL_DEL, sh->sock_fd, event) != 0)
					{
						w->stat.c[EPOLL_ERR]++;
						close_epoll_sock (w, event);
					}

					add_to_wait_list(w, sh);
				}
				else
				{
					event->events = EPOLLOUT;
					if (epoll_ctl(w->epoll_fd, EPOLL_CTL_MOD, sh->sock_fd, event) != 0)
					{
						++w->stat.c[EPOLL_ERR];
						close_epoll_sock (w, event);
					}
				}

				break;
			}
        }
	}

	return;
}

static Error_t do_server_loop(Worker_t* w)
{
	if ((w->epoll_fd = epoll_create1(0)) == -1)
	{
		LOG_ERR("WORKER_%d : Can't create epoll.\n", w->worker_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	struct epoll_event*	events = (struct epoll_event*) malloc(sizeof(struct epoll_event) * w->epoll_max_events);

	if (events == NULL)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for [%d] epoll events.\n", w->worker_id, w->epoll_max_events);
		return ERR_MEMORY_ALLOC;
	}

	while(!w->stop_loop)
	{
		int i, n;

		gettimeofday(&w->now, NULL);

		if(w->state == WORKER_STATE_CLEANUP &&
		   utils_timeval_diff(&w->now, &w->cleanup_start) > w->cleanup_timeout)
		{
#ifdef _TRACE
			printf ("WORKER_%d : Finished on cleanup timeout with %d open sockets\n", w->worker_id, w->sockets);
#endif
			break;
		}

		stat_refresh_worker_stat(&w->stat, w->worker_id, 1);

		accept_epoll(w);

		process_epoll_wait_list(w);

		n = epoll_wait(w->epoll_fd, events, w->epoll_max_events, w->epoll_timeout);

		/* Process requests and responses in a separate loops for more efficient CPU cache usage */

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLIN)
			{
				read_epoll(w, &events[i]);
			}
		}

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLOUT)
			{
				write_epoll(w, &events[i]);
			}
			else if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP))
			{
				if (events[i].events & EPOLLERR)
					w->stat.c[SOCK_ERR]++;

				if (events[i].events & EPOLLHUP)
					w->stat.c[SOCK_HUP]++;

				close_epoll_sock (w, &events[i]);
				continue;
			}

		} /* for */
	} /* while */

	free(events);

	return ERR_OK;
}

static Error_t do_client_loop(Worker_t* w)
{
	Error_t		rc;

	if ((w->epoll_fd = epoll_create1(0)) == -1)
	{
		LOG_ERR("WORKER_%d : Can't create epoll.\n", w->worker_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	struct epoll_event*	events = (struct epoll_event*) malloc(sizeof(struct epoll_event) * w->epoll_max_events);

	if (events == NULL)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for [%d] epoll events.\n", w->worker_id, w->epoll_max_events);
		return ERR_MEMORY_ALLOC;
	}

	if ((rc = do_start_step(w)) != ERR_OK)
		return ERR_GENERIC;


	while(!w->stop_loop)
	{
		int i, n;

		gettimeofday(&w->now, NULL);

		process_epoll_wait_list(w);

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
				return ERR_GENERIC;
		}

		/*  Processing ReqPerConnection and/or open new sockets to replace closed ones */
		if(w->sockets < w->sock_max_count && w->state == WORKER_STATE_IN_PROGRESS)
			add_sockets(w);

		stat_refresh_worker_stat(&w->stat, w->worker_id, w->full_load);

		n = epoll_wait(w->epoll_fd, events, w->epoll_max_events, w->epoll_timeout);

		/* Process requests and responses in a separate loops for more efficient CPU cache usage */

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLIN)
			{
				read_epoll(w, &events[i]);
			}
		}

		for (i = 0; i < n; i++)
		{
			if (events[i].events & EPOLLOUT)
			{
				write_epoll(w, &events[i]);

				if (w->stat.c[MSG_SENT] >= w->req_count &&
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
					w->stat.c[SOCK_HUP]++;

				close_epoll_sock (w, &events[i]);
				continue;
			}

		} /* for */

	} /* while */

	free(events);

	return ERR_OK;
}

#endif

/**************************************************************************************************/

static Error_t worker_init_common (Worker_t* w, uint16_t id, unsigned long affinity)
{
	Error_t		rc;

	memset(w, 0, sizeof(Worker_t));

	w->worker_id 			= id;
	w->sockets				= 0;
	w->state 				= WORKER_STATE_IDLE;
	w->sock_max_count		= 0;
	w->affinity				= affinity;

	/* We keep copies of global parameters in order to optimize cache usage */
	w->req_per_conn			= g_params.req_per_conn;
	w->delay				= g_params.delay;
	w->parse_every_http		= g_params.parse_every_http;
	w->start_steps			= g_params.start_steps;
	w->step_time			= g_params.step_time;
	w->cleanup_timeout 		= g_params.worker_cleanup_timeout;

#ifdef _SPOOL
	w->sock_list_size		= 1000;
#else
	w->epoll_fd				= -1;
	w->epoll_max_events		= g_params.epoll_max_events;
	w->epoll_timeout		= g_params.epoll_timeout;
#endif

	w->read_buff			= (char*)malloc(g_params.read_buff_size);
	w->read_buff_size 		= g_params.read_buff_size;

	if (w->read_buff == NULL)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for read buffer\n", w->worker_id);
		return ERR_MEMORY_ALLOC;
	}

	rc = utils_hash_init(&w->sock_hash, WORKER_SOCK_HASH_SIZE);
	if(rc != ERR_OK)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for socket hash\n", w->worker_id);
		return rc;
	}
	return ERR_OK;
}

Error_t worker_init_as_server (Worker_t* w, uint16_t id, unsigned long affinity)
{
	Error_t		rc;

	rc = worker_init_common(w, id, affinity);

	if (rc != ERR_OK)
		return rc;

	w->type 		= WORKER_TYPE_SERVER;
	w->full_load 	= 1;

	/* Disable request per connection on the server */
	w->req_per_conn = 0;

	fifo_ring_init(&w->accept_q, g_params.accept_q_size);

	if(response_create_response(&w->write_buff) != ERR_OK)
	{
		LOG_ERR("WORKER_%d : Can't allocate memory for response\n", w->worker_id);
		return ERR_MEMORY_ALLOC;
	}

	w->write_buff_size = strlen(w->write_buff);

#ifdef _TRACE
	printf("WORKER_%d : Server worker initialized with affinity [%lx]\n", w->worker_id, w->affinity);
#endif

	return ERR_OK;
}

Error_t worker_init_as_client(Worker_t* w, uint16_t id, uint32_t conn_count, uint64_t req_count, unsigned long affinity)
{
	Error_t		rc;

	rc = worker_init_common(w, id, affinity);

	if (rc != ERR_OK)
		return rc;

	w->type					= WORKER_TYPE_CLIENT;

	w->conn_count 			= conn_count;
	w->req_count 			= req_count;
	w->step_state			= STEP_STATE_INIT;
	w->step					= 1;
	w->tuple_list			= NULL;


#ifdef _TRACE
	printf ("WORKER_%d : Client worker initialized with params : conn_count=%d, request_count=%lu, affinity=%lx \n",
			w->worker_id, w->conn_count, w->req_count, w->affinity );
#endif

	return ERR_OK;
}

void*  worker_do_work(void* param)
{
	Worker_t* 	w = (Worker_t*)param;
	Error_t		rc;

	if(w->affinity > 0)
	{
		if(pthread_setaffinity_np(w->thread, sizeof(w->affinity), &w->affinity) != 0)
		{
			LOG_ERR("WORKER_%d : Can't set affinity [%lx].\n", w->worker_id, w->affinity);
			dispatcher_on_finish(THREAD_TYPE_WORKER);
			return NULL;
		}
#ifdef _TRACE
		printf ("WORKER_%d : Setting CPU affinity mask [%lx].\n", w->worker_id,  w->affinity);
#endif
	}

	w->state 		= WORKER_STATE_IN_PROGRESS;
	w->stop_loop	= 0;

	if(w->type == WORKER_TYPE_CLIENT)
		rc = do_client_loop(w);
	else
		rc = do_server_loop(w);

	if(rc != ERR_OK)
	{
		dispatcher_on_finish(THREAD_TYPE_WORKER);
		return NULL;
	}

	stat_refresh_worker_stat(&w->stat, w->worker_id, w->full_load);

	free_tuple_list(w->tuple_list);

#ifdef _TRACE
		printf ("WORKER_%d : Closed.\n", w->worker_id);
#endif

	dispatcher_on_finish(THREAD_TYPE_WORKER);

	return NULL;
}


void worker_accept(Worker_t*w, void* sock)
{
	fifo_ring_add(&w->accept_q, sock);
}

Error_t worker_add_tuple (Worker_t* w, Tuple_t* t)
{
	Error_t rc;

	if((rc = request_create_request(&t->req_buff, &t->url)) != ERR_OK)
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

void worker_close(Worker_t* w)
{
	w->stop_loop = 1;

	if(w->type == WORKER_TYPE_SERVER)
		w->state = WORKER_STATE_CLEANUP;
}

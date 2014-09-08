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

#include "listener.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
//#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#include "dispatcher.h"
#include "log.h"


/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/


#ifdef _SPOOL

static void close_foreign_socks(Listener_t* l)
{
	struct spool_sock spool_sock;

	while ((spool_sock.sock_h = (long)fifo_ring_get(&l->close_q)) != 0)
	{
		int res;

		LOG_DEBUG("LISTENER_%d : Closing socket [%lx]\n", l->listener_id, spool_sock.sock_h);

		res = ioctl(l->spool_fd, SPOOL_IO_CLOSESOCK, &spool_sock);
		if(res < 0)
		{
			LOG_ERR("LISTENER_%d  : Spool ioctl failed on socket close with error [%d]\n", l->listener_id, res);
		}
	}
}

#endif

static Error_t open_listening_socket(Listener_t* l)
{
	int res;

#ifdef _SPOOL

	l->spool_fd = open("/dev/spool", O_RDWR);

	if (l->spool_fd < 0)
	{
		LOG_ERR("LISTENER_%d : Can't open spool file. Error %d : %s.\n", l->listener_id, errno, strerror(errno));
		return ERR_CANT_OPEN_FILE;
	}

	/* Listening address and port were set up in listener_init function */

	l->lis_sock.flags 		= SPOLL_FLAG_LISTENING_SOCK;
	l->lis_sock.backlog 	= l->lis_backlog;

	res = ioctl(l->spool_fd, SPOOL_IO_ADDSOCK, &l->lis_sock);
	if(res < 0)
	{
		LOG_ERR("LISTENER_%d: Spool ioctl failed on listen with error [%d]\n", l->listener_id, res);
		return ERR_CANT_CREATE_SOCKET;
	}

	l->accept.listening_sock_h = l->lis_sock.sock_h;
#else

    if ((l->lis_sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
    	LOG_ERR("LISTENER_%d : Can't create listening socket. Error %d : %s. Connection terminated.\n", l->listener_id, errno, strerror(errno))
        return ERR_CANT_CREATE_SOCKET;
    }

    if ((res = bind(l->lis_sock_fd, (struct sockaddr*)&l->lis_addr, sizeof(l->lis_addr))) < 0)
    {
    	LOG_ERR("LISTENER_%d : Can't bind listening socket. Error %d : %s. Connection terminated.\n", l->listener_id, errno, strerror(errno))
        return ERR_CANT_CREATE_SOCKET;
    }

    if((res = listen(l->lis_sock_fd, l->lis_backlog)) < 0)
    {
    	LOG_ERR("LISTENER_%d : Can't listen on socket. Error %d : %s. Connection terminated.\n", l->listener_id, errno, strerror(errno))
        return ERR_CANT_CREATE_SOCKET;
    }

#endif

    return ERR_OK;
}


static Error_t do_listen(Listener_t* l)
{

	WorkerListEntry_t* cur_worker = l->worker_list;

#ifdef _SPOOL
	int res, accepted = 0;

	l->accept.sock_list	= (struct spool_sock*)malloc(sizeof(struct spool_sock)*MAX_ACCEPTED_SOCKS);
	l->accept.size = MAX_ACCEPTED_SOCKS;

	if ((l->epoll_fd = epoll_create1(0)) == -1)
	{
		LOG_ERR("LISTENER_%d : Can't create epoll.\n", l->listener_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	struct epoll_event event;

	event.data.fd	= l->spool_fd;
	event.events	= EPOLLIN;

	if (epoll_ctl(l->epoll_fd, EPOLL_CTL_ADD, l->spool_fd, &event) != 0)
	{
		LOG_ERR("LISTENER_%d : Can't add event to epoll.\n", l->listener_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	while (!l->stop_loop)
	{
		int n;

		close_foreign_socks(l);

		n = epoll_wait(l->epoll_fd, &l->lis_event, 1, l->epoll_timeout);

		if(n<1)
			continue;

		res = ioctl(l->spool_fd, SPOOL_IO_ACCEPT, &l->accept);
		if(res < 0)
		{
			LOG_ERR("LISTENER_%d  : Spool ioctl failed on accept with error [%d]\n",l->listener_id, res);
			return ERR_ACCEPT_FAILED;
		}

		/* Put the list of accepted sockets to the worker.
		   The worker will be responsible for freeing memory */
		if (l->accept.size > 0)
		{
			accepted+=l->accept.size;

			LOG_DEBUG("LISTENER_%d : Accepted %d sockets\n", l->listener_id, accepted);

			worker_accept(cur_worker->worker, l->accept.sock_list);

			l->accept.sock_list	= (struct spool_sock*)malloc(sizeof(struct spool_sock)*MAX_ACCEPTED_SOCKS);
			l->accept.size = MAX_ACCEPTED_SOCKS;
		}

		if(cur_worker->next == NULL)
			cur_worker = l->worker_list;
		else
			cur_worker = cur_worker->next;
	}

#else

	if ((l->epoll_fd = epoll_create1(0)) == -1)
	{
		LOG_ERR("LISTENER_%d : Can't create epoll.\n", l->listener_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	struct epoll_event event;

	event.data.fd	= l->lis_sock_fd;
	event.events	= EPOLLIN;

	if (epoll_ctl(l->epoll_fd, EPOLL_CTL_ADD, l->lis_sock_fd, &event) != 0)
	{
		LOG_ERR("LISTENER_%d : Can't add event to epoll.\n", l->listener_id);
		return ERR_CANT_CREATE_EPOLL;
	}

	while (!l->stop_loop)
	{
		int n = epoll_wait(l->epoll_fd, &l->lis_event, 1, l->epoll_timeout);

		if(n<1)
			continue;

		if (l->lis_event.events & EPOLLIN)
		{
			int fd = accept(l->lis_sock_fd, NULL, 0);

			if(fd >= 0)
			{
				int* sock = malloc(sizeof(int));
				*sock = fd;

				worker_accept(cur_worker->worker, sock);

				if(cur_worker->next == NULL)
					cur_worker = l->worker_list;
				else
					cur_worker = cur_worker->next;
			}
		}
		else
		{
			LOG_ERR("LISTENER_%d : Epoll error.\n", l->listener_id);
		}
	}

	close(l->epoll_fd);
	close(l->lis_sock_fd);

#endif

	return ERR_OK;
}

/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/


Error_t listener_init (Listener_t* l,  uint16_t id, struct sockaddr_in* addr, unsigned long affinity)
{
	memset(l, 0, sizeof(Listener_t));

#ifdef _SPOOL
	memcpy(&l->lis_sock.local, addr, sizeof(struct sockaddr_in));
#else
	memcpy(&l->lis_addr, addr, sizeof(struct sockaddr_in));
#endif

	l->affinity			= affinity;
	l->listener_id		= id;
	l->lis_backlog		= 10000;  // TODO - Make configurable
	l->epoll_timeout 	= g_params.epoll_timeout;

	fifo_ring_init(&l->close_q, g_params.accept_q_size);

#ifdef _TRACE
	printf ("LISTENER_%d : Initialized with params : affinity=%lx \n", l->listener_id, l->affinity);
#endif

	return ERR_OK;
}

Error_t listener_add_worker(Listener_t* l, Worker_t* w)
{
	WorkerListEntry_t* we = (WorkerListEntry_t*)malloc(sizeof(WorkerListEntry_t));

	if(we == NULL)
		return ERR_MEMORY_ALLOC;

	we->worker 		= w;
	we->next 		= l->worker_list;
	l->worker_list	= we;

	return ERR_OK;
}


void*  listener_do_work (void* param)
{
	Listener_t* l = (Listener_t*)param;

	if(l->worker_list == NULL)
	{
		LOG_ERR("LISTENER_%d : At least one server worker should be added.\n", l->listener_id);
		dispatcher_on_finish(THREAD_TYPE_LISTENER);
		return NULL;
	}

	if(l->affinity > 0)
	{
		if(pthread_setaffinity_np(l->thread, sizeof(l->affinity), &l->affinity) != 0)
		{
			LOG_ERR("LISTENER_%d : Can't set affinity [%lx].\n", l->listener_id, l->affinity);
			dispatcher_on_finish(THREAD_TYPE_LISTENER);
			return NULL;
		}
#ifdef _TRACE
		printf ("LISTENER_%d  : Setting CPU affinity mask [%lx].\n", l->listener_id, l->affinity);
#endif
	}

	if (open_listening_socket(l) != ERR_OK)
	{
		dispatcher_on_finish(THREAD_TYPE_LISTENER);
		return NULL;
	}

#ifdef _TRACE
		printf ("LISTENER_%d  : Start listening.\n", l->listener_id);
#endif


	do_listen(l);

	return NULL;
}

void listener_close(Listener_t* l)
{
#ifdef _SPOOL
	ioctl(l->spool_fd, SPOOL_IO_CLOSESOCK, &l->lis_sock);
	close(l->spool_fd);
	pthread_cancel(l->thread);
#else
	l->stop_loop = 1;
#endif

	dispatcher_on_finish(THREAD_TYPE_LISTENER);

#ifdef _TRACE
	printf ("LISTENER_%d : Closed.\n", l->listener_id);
#endif

}

void listener_close_foreign_sock(Listener_t* l, long sock_h)
{
	fifo_ring_add(&l->close_q, (void*)sock_h);
}

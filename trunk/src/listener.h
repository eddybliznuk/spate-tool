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

#ifndef _LISTENER_H_
#define _LISTENER_H_

#ifdef _SPOOL
#include "spool.h"
#endif

#include "params.h"

#include <sys/epoll.h>

#include "stat.h"
#include "general_defs.h"
#include "fifo_ring.h"
#include "worker.h"

typedef struct WorkerListEntry_
{
	Worker_t*					worker;
	struct WorkerListEntry_*	next;

} WorkerListEntry_t;

typedef struct Listener_
{
#ifdef _SPOOL
	struct spool_sock	lis_sock;
	struct spool_accept accept;
	int					spool_fd;
	struct spool_sock	accepted_socks[MAX_ACCEPTED_SOCKS];
#endif

	struct sockaddr_in  lis_addr;
	int					lis_sock_fd;
	int					epoll_fd;
	struct epoll_event	lis_event;
	uint32_t			epoll_timeout;

	uint8_t				stop_loop;
	uint8_t				listener_id;
	uint32_t			lis_backlog;
	pthread_t			thread;
	unsigned long		affinity;

	WorkerListEntry_t*	worker_list;
	FifoRing_t			close_q;

} Listener_t;

Error_t listener_init (Listener_t* l,  uint16_t id, struct sockaddr_in* addr, unsigned long affinity);
void* listener_do_work(void* param);
Error_t listener_add_worker(Listener_t* l, Worker_t* w);
void listener_close(Listener_t* l);
void listener_close_foreign_sock(Listener_t* l, long sock_h);

#endif /* _LISTENER_H_ */

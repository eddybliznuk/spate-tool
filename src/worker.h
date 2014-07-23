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

#ifndef _WORKER_H_
#define _WORKER_H_

#include "params.h"

#include "stat.h"
#include "general_defs.h"

#include <pthread.h>
#include <time.h>
#include <sys/epoll.h>

#define STR_BUF_LEN				1024
#define MAX_WRITE_SIZE			10240

typedef enum Worker_State
{
	WORKER_STATE_IDLE,
	WORKER_STATE_IN_PROGRESS,
	WORKER_STATE_CLEANUP           /* Don't send new requests, finish processing responses within 'cleanup_timeout' */

} Worker_State_e;

typedef enum StepState_
{
	STEP_STATE_INIT,
	STEP_STATE_DO_STEPS,
	STEP_STATE_STABLE

}  StepState_e;

/* Optimized for 4 cache lines (64 byte each) */
typedef struct Socket_
{
	int					sock_fd;

	uint32_t			req_buf_size;   /* size of static request in the tuple */
	uint32_t			write_buf_size; /* bytes left to write; when it is 0 the request is finished */

	uint32_t			resp_rcvd_size;
	uint32_t			first_read;
	uint32_t			requests;

	char*				req_buf;     /* pointer to static request buffer in the tuple */
	char*				write_buf;   /* position in the request buffer where the next write will be from */

	/* Wait list pointers */
	struct Socket_*		next;
	struct Socket_*		prev;

	struct timeval		next_request_time;

	Parser_t			parser;

} Socket_t;


/* Optimized for 3 cache lines (64 byte each) */
typedef struct Worker_
{
	int					epoll_fd;
	uint8_t				stop_loop;
	uint8_t				state;
	uint8_t				full_load;
	uint8_t				parse_every_resp;
	uint32_t			sockets;
	uint32_t			sock_max_count;
	uint32_t			epoll_timeout;
	uint32_t			conn_count;

	struct timeval		now;

	char*				read_buf;   /* pointer to a shared buffer for all reads */
	uint32_t			read_buf_size;
	uint32_t			req_per_conn;
	uint64_t	 		req_count;

	/* 64 byte - first cache line */

	Stat_t				stat;

	/* 128 byte - second cache line */

	uint32_t			delay;
	uint32_t			step_time;

	int64_t				cleanup_timeout;  /* microseconds */
	struct timeval		cleanup_start;

	uint32_t			step;
	struct timeval		last_step_time;
	StepState_e			step_state;

	Socket_t*			wait_list_head;
	Socket_t*			wait_list_tail;

	TupleList_t*		tuple_list;
	TupleList_t*		cur_tuple;

	uint16_t			start_steps;
	uint16_t			worker_id;
	uint32_t			epoll_max_events;

	pthread_t			thread;
	unsigned long		affinity;


} Worker_t ;


Error_t worker_init (Worker_t* w, uint16_t id, uint32_t conn_count, uint64_t req_count,  unsigned long affinity);
Error_t worker_add_tuple (Worker_t* w, Tuple_t* t);
void* worker_do_work(void* param);


#endif /* _WORKER_H_ */

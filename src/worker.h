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

#ifdef _SPOOL
#include "spool.h"
#endif

#include "params.h"

#include "stat.h"
#include "general_defs.h"
#include "fifo_ring.h"
#include "utils.h"

#include <pthread.h>
#include <time.h>


#define STR_BUF_LEN				1024
#define MAX_WRITE_SIZE			10240
#define SOCK_LIST_COUNT			1000
#define WORKER_SOCK_HASH_SIZE	10000

#define SOCK_FLAG_FOREIGN		0x0001

typedef enum WorkerType_
{
	WORKER_TYPE_SERVER,
	WORKER_TYPE_CLIENT

}  WorkerType_e;

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
#ifdef _SPOOL
	struct spool_sbd 	wbd;
	struct spool_sbd 	rbd;
#else
	char*				write_buff;
	char*				cur_write_buff;
	uint32_t			write_buff_size;
	uint32_t			cur_write_buff_size;
#endif

	uint32_t			flags;

	long				sock_fd;

	uint32_t			read_buff_size;
	uint32_t			rcvd_size;

	uint32_t			first_read;
	uint32_t			requests;

	char*				read_buff;

	/* Wait list pointers */
	struct Socket_*		next;
	struct Socket_*		prev;

	struct timeval		next_action_time;

	Parser_t			parser;

} Socket_t;


typedef struct Worker_
{
#ifdef _SPOOL
	int					spool_fd;
	struct spool_sbd*	rbd_head[SOCK_LIST_COUNT];
	struct spool_sbd*	wbd_head[SOCK_LIST_COUNT];
	uint32_t			rbd_list_size[SOCK_LIST_COUNT];
	uint32_t			wbd_list_size[SOCK_LIST_COUNT];
	uint32_t			cur_rbd_list;
	uint32_t			cur_wbd_list;
	uint32_t			sock_list_size;
#else
	int					epoll_fd;
	uint32_t			epoll_max_events;
	uint32_t			epoll_timeout;
#endif

	uint32_t			sockets;
	uint32_t			sock_max_count;

	uint8_t				type;
	uint8_t				stop_loop;
	uint8_t				state;
	uint8_t				full_load;
	uint8_t				parse_every_http;
	uint8_t				parse_every_resp;

	char*				write_buff;
	char*				read_buff;

	uint32_t			write_buff_size;
	uint32_t			read_buff_size;

	uint64_t	 		req_count;
	uint64_t	 		req_processed;
	uint64_t	 		resp_processed;

	uint32_t			delay;
	uint32_t			step_time;

	int64_t				cleanup_timeout;  /* microseconds */

	uint32_t			req_per_conn;
	uint32_t			step;
	uint32_t			conn_count;

	uint16_t			start_steps;
	uint16_t			worker_id;

	struct timeval		now;
	struct timeval		cleanup_start;
	struct timeval		last_step_time;

	StepState_e			step_state;

	Socket_t*			wait_list_head;
	Socket_t*			wait_list_tail;

	TupleList_t*		tuple_list;
	TupleList_t*		cur_tuple;

	pthread_t			thread;
	unsigned long		affinity;

	EggBasket_t 		step_eb;
	FifoRing_t			accept_q;
	Hash_t				sock_hash;

	struct Listener_*	listener;

	Stat_t				stat;

} Worker_t ;

Error_t worker_init_as_server (Worker_t* w, uint16_t id, unsigned long affinity);
Error_t worker_init_as_client (Worker_t* w, uint16_t id, uint32_t conn_count, uint64_t req_count, unsigned long affinity);

Error_t worker_add_tuple (Worker_t* w, Tuple_t* t);
void worker_accept(Worker_t*w, void* sock);
void* worker_do_work(void* param);
void worker_close(Worker_t* w);

Socket_t* worker_get_socket_handler();
void worker_free_tuple_list(TupleList_t* tl);




void worker_read_spool(Worker_t* w, uint32_t list_id);
void worker_write_spool(Worker_t* w, uint32_t list_id);
void worker_clean_up_spool_socks(Worker_t* w);

#endif /* _WORKER_H_ */

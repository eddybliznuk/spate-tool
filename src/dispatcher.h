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

#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include <pthread.h>

#include "worker.h"
#include "listener.h"

typedef enum ThreadType_
{
	THREAD_TYPE_LISTENER,
	THREAD_TYPE_WORKER,
	THREAD_TYPE_COUNT

} ThreadType_e;

typedef struct Dispatcher_
{
	/* Pointer to the array of Worker_t structures */
	Worker_t*			workers;

	/* Pointer to the array of Listener_t structures */
	Listener_t*			listeners;

	uint16_t			thread_count[THREAD_TYPE_COUNT];
	uint16_t			cur_affinity; /* for distributing affinity from the list among listeners and workers */

	pthread_mutex_t		mutex;

} Dispatcher_t ;


Error_t dispatcher_init_and_start_workers ();
void dispatcher_on_finish(ThreadType_e thread_type);
void dispatcher_close_all();

#endif /* _DISPATCHER_H_ */

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
#include "stat.h"

typedef struct Dispatcher_
{
	/* Pointer to the array of Worker_t structures */
	Worker_t*			workers;
	uint16_t			worker_count;

	pthread_mutex_t		mutex;

} Dispatcher_t ;


Error_t dispatcher_init_and_start_workers ();
void dispatcher_on_finish();

#endif /* _DISPATCHER_H_ */

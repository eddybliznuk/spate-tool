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

#ifndef _FIFO_RING_H_
#define _FIFO_RING_H_

#include "general_defs.h"

#include <pthread.h>

typedef struct FifoRing_
{
	void**		head;
	void**		tail;

	void**		start;
	void**		end;

	uint32_t	size;

	pthread_mutex_t	mutex;

} FifoRing_t;

Error_t fifo_ring_init(FifoRing_t* fr, uint32_t size);
Error_t fifo_ring_add (FifoRing_t* fr, void* data);
void* fifo_ring_get (FifoRing_t* fr);


#endif /*  _FIFO_RING_H_ */

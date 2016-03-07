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

#include "fifo_ring.h"
#include "stdlib.h"


Error_t fifo_ring_init(FifoRing_t* fr, uint32_t size)
{
	fr->start = (void**)malloc(sizeof(void*)*size);

	if(fr->start == NULL)
		return ERR_MEMORY_ALLOC;

	fr->end = &fr->start[size-1];
	fr->head = fr->tail = fr->start;
	fr->size = 0;

	pthread_mutex_init(&fr->mutex, NULL);

	return ERR_OK;
}

Error_t fifo_ring_add (FifoRing_t* fr, void* data)
{
	Error_t rc = ERR_OK;

	pthread_mutex_lock(&fr->mutex);

	if(fr->tail == fr->head && fr->size > 0)
		rc = ERR_OUT_OF_BOUNDS;
	else
	{
		*fr->tail = data;
		++fr->size;

		if(fr->tail == fr->end)
			fr->tail = fr->start;
		else
			++fr->tail;
	}

	pthread_mutex_unlock(&fr->mutex);

	return rc;
}

void* fifo_ring_get (FifoRing_t* fr)
{
	void* rc = NULL;

	pthread_mutex_lock(&fr->mutex);

	if(fr->size > 0)
	{
		rc = *fr->head;
		--fr->size;

		if(fr->head == fr->end)
			fr->head = fr->start;
		else
			++fr->head;
	}

	pthread_mutex_unlock(&fr->mutex);

	return rc;
}


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

#include "dispatcher.h"

#include "params.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Dispatcher_t d;

/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/

static Error_t create_worker_thread (Worker_t* w)
{
	int res;

	if ((res=pthread_create(&w->thread, NULL, worker_do_work, (void*)w)) != 0)
		return ERR_CANT_CREATE_THREAD;

	 pthread_detach(w->thread);

	 ++d.worker_count;

	return ERR_OK;
}

/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/

Error_t dispatcher_init_and_start_workers ()
{
	Error_t		rc;

	d.worker_count	= 0;
	d.workers 		= (Worker_t*)malloc(sizeof(Worker_t) * g_params.worker_count);

	EggBasket_t eb_con, eb_req, eb_tuple;
	utils_eggbasket_init(&eb_con, g_params.conn_count, g_params.worker_count);
	utils_eggbasket_init(&eb_req,  g_params.req_count, g_params.worker_count);
	utils_eggbasket_init(&eb_tuple,  g_params.tuple_count, g_params.worker_count);

	TupleList_t* tl = g_params.tuple_list;

	uint32_t i;

	for(i=0; i<g_params.worker_count; i++)
	{
		if ((rc = worker_init(&d.workers[i], i, utils_eggbasket_get(&eb_con), utils_eggbasket_get(&eb_req), g_params.affinity_list[i])) != ERR_OK)
		{
			free(d.workers);
			return rc;
		}

		int touple_count = utils_eggbasket_get(&eb_tuple);

		if(touple_count == 0)
		{
			/* Round Robin on tuple list */
			utils_eggbasket_init(&eb_tuple,  g_params.tuple_count, g_params.worker_count);

			tl				= g_params.tuple_list;
			touple_count 	= utils_eggbasket_get(&eb_tuple);
		}

		for(uint32_t j=0; j<touple_count; ++j)
		{
			if ((rc=worker_add_tuple (&d.workers[i], tl->t)) != ERR_OK)
			{
				free(d.workers);
				return rc;
			}

			if(tl->next != NULL)
				tl = tl->next;
			else
				break;
		}
	}

	pthread_mutex_init(&d.mutex, NULL);

	/* We create worker threads and start all workers separately
	 * trying to achieve quasi-simultaneous start
	 */

	stat_start_time_count();

	for(i=0; i<g_params.worker_count; i++)
	{
		create_worker_thread (&d.workers[i]);
	}

	fprintf(stdout, "Starting the test...\n\n");

	return ERR_OK;
}

void dispatcher_on_finish()
{
	pthread_mutex_lock(&d.mutex);

	if (--d.worker_count == 0 )
	{
		stat_print_total_stat();

#ifdef _TRACE
		printf ("DISPATCHER : All workers finished\n");
#endif
		exit (0);
	}

	pthread_mutex_unlock(&d.mutex);
}



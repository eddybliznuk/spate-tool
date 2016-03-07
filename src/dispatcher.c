/*
    SPATE - HTTP performance test tool

    Copyright (C) 2014  Edward Blizniuk (known also as Ed Blake)

    This program is free software: you can redistribute it and/or modify
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
#include "listener.h"
#include "worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Dispatcher_t d;

/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/

static Error_t init_listeners()
{
	uint32_t i, j, cur_worker = 0;
	EggBasket_t eb_w;

	utils_eggbasket_init(&eb_w, g_params.worker_count, g_params.listener_count);

	d.thread_count[THREAD_TYPE_LISTENER]	= 0;
	d.listeners 		= (Listener_t*)malloc(sizeof(Listener_t) * g_params.listener_count);

	TupleList_t* lt = g_params.listener_list;

	for(i=0; (i<g_params.listener_count) && (lt != NULL); i++)
	{
		Listener_t* lis = &d.listeners[i];

		listener_init(lis, i, &lt->t->local_addr, g_params.affinity_list[d.cur_affinity++]);

		if ((pthread_create(&lis->thread, NULL, listener_do_work, (void*)lis)) != 0)
			return ERR_CANT_CREATE_THREAD;

		pthread_detach(lis->thread);
		++d.thread_count[THREAD_TYPE_LISTENER];

		for (j=0; (j<utils_eggbasket_get(&eb_w)) && (cur_worker < d.thread_count[THREAD_TYPE_WORKER]); j++)
		{
			Error_t rc;
			rc = listener_add_worker(lis, &d.workers[cur_worker]);
			if(rc != ERR_OK)
				return rc;

			d.workers[cur_worker].listener = lis;

			++cur_worker;
		}

		lt = lt->next;
	}

	return ERR_OK;
}

static Error_t init_server_workers()
{
	Error_t rc;
	uint32_t i;

	d.thread_count[THREAD_TYPE_WORKER]	= 0;
	d.workers = (Worker_t*)malloc(sizeof(Worker_t) * g_params.worker_count);

	stat_start_time_count();

	for(i=0; i<g_params.worker_count; i++)
	{
		Worker_t* w = &d.workers[i];

		if ((rc = worker_init_as_server(w, i, g_params.affinity_list[d.cur_affinity++])) != ERR_OK)
		{
			free(d.workers);
			return rc;
		}

		if ((pthread_create(&w->thread, NULL, worker_do_work, (void*)w)) != 0)
			return ERR_CANT_CREATE_THREAD;

		 pthread_detach(w->thread);

		 ++d.thread_count[THREAD_TYPE_WORKER];
	}

	return ERR_OK;
}

static Error_t init_client_workers()
{
	Error_t rc;
	uint32_t i;

	d.thread_count[THREAD_TYPE_WORKER]	= 0;
	d.workers	= (Worker_t*)malloc(sizeof(Worker_t) * g_params.worker_count);

	EggBasket_t eb_con, eb_req, eb_tuple;
	utils_eggbasket_init(&eb_con, g_params.conn_count, g_params.worker_count);
	utils_eggbasket_init(&eb_req,  g_params.req_count, g_params.worker_count);
	utils_eggbasket_init(&eb_tuple,  g_params.tuple_count, g_params.worker_count);

	TupleList_t* tl = g_params.tuple_list;

	for(i=0; i<g_params.worker_count; i++)
	{
		if ((rc = worker_init_as_client (&d.workers[i],
										 i,
										 utils_eggbasket_get(&eb_con),
										 utils_eggbasket_get(&eb_req),
										 g_params.affinity_list[i])) != ERR_OK)
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

	/* We create worker threads and start all workers separately
	 * trying to achieve quasi-simultaneous start
	 */

	stat_start_time_count();

	for(i=0; i<g_params.worker_count; i++)
	{
		Worker_t* w = &d.workers[i];

		if ((pthread_create(&w->thread, NULL, worker_do_work, (void*)w)) != 0)
			return ERR_CANT_CREATE_THREAD;

		 pthread_detach(w->thread);

		 ++d.thread_count[THREAD_TYPE_WORKER];
	}

	return ERR_OK;
}

/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/

Error_t dispatcher_init_and_start_workers ()
{
	Error_t	rc;

	pthread_mutex_init(&d.mutex, NULL);

	d.cur_affinity = 0;

	if (g_params.mode == SPATE_MODE_SERVER)
	{
		if ((rc = init_server_workers()) != ERR_OK)
			return rc;

		rc =  init_listeners();
	}
	else
		rc = init_client_workers();

	if (rc == ERR_OK)
		fprintf(stdout, "\nStarting the test...\n\n");

	return rc;
}

void dispatcher_on_finish(ThreadType_e thread_type)
{
	pthread_mutex_lock(&d.mutex);

	--d.thread_count[thread_type];

	if (d.thread_count[THREAD_TYPE_LISTENER] == 0 &&
		d.thread_count[THREAD_TYPE_WORKER] == 0)
	{
		stat_print_total_stat();

#ifdef _TRACE
		printf ("DISPATCHER : All threads finished\n");
#endif
		exit (0);
	}

	pthread_mutex_unlock(&d.mutex);
}

void dispatcher_close_all()
{
	int i;

	for(i=0; i<	d.thread_count[THREAD_TYPE_WORKER]; i++)
	{
		worker_close(&d.workers[i]);
	}

	sleep(1);

	for(i=0; i<d.thread_count[THREAD_TYPE_LISTENER]; i++)
	{
		listener_close(&d.listeners[i]);
	}
}


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

#include "stat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "params.h"
#include "utils.h"
#include "log.h"

static GlobalStat_t g_stat;

static char* counter_name[] = {
	"Sockets opened",
	"Sockets closed",
	"Epoll errors",
	"Socket errors",
	"Socket hangups",
	"Bytes sent",
	"Bytes received",
	"Messages sent",
	"Messages received"
};

static char* rate_counter_name[] = {
	"Requests per second",
	"Bytes sent per second",
	"Bytes received per second"
};

/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/

static void summarize()
{
	Stat_t 			prev_total;
	struct timeval 	prev_period_start;
	uint64_t		period_time;

	memcpy(&prev_period_start, &g_stat.period_start_time, sizeof(prev_period_start));
	gettimeofday(&g_stat.period_start_time, NULL);

	period_time = utils_timeval_diff (&g_stat.period_start_time, &prev_period_start);
	g_stat.progress_time 	= utils_timeval_diff (&g_stat.period_start_time, &g_stat.start_time);

	memcpy(&prev_total, &g_stat.total, sizeof(Stat_t));
	memset(&g_stat.total, 0, sizeof(Stat_t));

	int workers_ready = 0;

	// Refresh cumulative counters using the updated worker statistics
	for (uint32_t i=0; i<g_params.worker_count; i++)
	{
		for(uint32_t j=0; j<COUNTER_COUNT; j++)
			g_stat.total.c[j] += g_stat.worker_stat[i].c[j];

		if(g_stat.worker_is_ready[i] == 1)
			++workers_ready;
	}

	// Calculate rate statistics for the sample period
	g_stat.rate_period[REQ_PER_SEC] 		= (uint64_t)((double)(g_stat.total.c[MSG_SENT]-prev_total.c[MSG_SENT])/(period_time/1000000.0));
	g_stat.rate_period[BYTES_SENT_PER_SEC]	= (uint64_t)((double)(g_stat.total.c[BYTES_SENT]-prev_total.c[BYTES_SENT])/(period_time/1000000.0));
	g_stat.rate_period[BYTES_RCVD_PER_SEC]	= (uint64_t)((double)(g_stat.total.c[BYTES_RCVD]-prev_total.c[BYTES_RCVD])/(period_time/1000000.0));

	if(workers_ready == g_params.worker_count)
	{
		// We will calculate average value of rates in all samples taken after step-by-step period
		// was finished (if step-by-step was used at all).
		// These averages will be shown in the stat summary in the end if the test
		g_stat.rate_average[REQ_PER_SEC] += g_stat.rate_period[REQ_PER_SEC];
		g_stat.rate_average[BYTES_SENT_PER_SEC] += g_stat.rate_period[BYTES_SENT_PER_SEC];
		g_stat.rate_average[BYTES_RCVD_PER_SEC] += g_stat.rate_period[BYTES_RCVD_PER_SEC];
		++g_stat.sample_count;
	}

}

static Error_t print_sample()
{
	FILE* f;

	if(g_stat.first_sample)
	{
		f = fopen (g_params.sample_path, "w+");
		g_stat.first_sample = 0;
	}
	else
	{
		f = fopen (g_params.sample_path, "a");
	}

	if (f == NULL)
	{
		LOG_ERR("Can't open statistics sample file [%s]\n", g_params.sample_path);
		return ERR_CANT_OPEN_FILE;
	}

	fprintf(f, "%lu", g_stat.progress_time);

	for (int i=0; i<COUNTER_COUNT; i++)
	{
		fprintf(f, ", %lu", g_stat.total.c[i]);
	}

	for (int i=0; i<RATE_COUNTER_COUNT; i++)
	{
		fprintf(f, ", %lu", g_stat.rate_period[i]);
	}

	fprintf(f, "\n");

	fclose(f);

	return ERR_OK;
}

/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/

Error_t stat_init()
{
	memset (&g_stat, 0, sizeof(g_stat));

	// Allocate memory for array of pointers to worker statistics
	g_stat.worker_stat  = (Stat_t*)malloc(sizeof(Stat_t) * g_params.worker_count);
	g_stat.worker_is_ready  = (uint8_t*)malloc(sizeof(uint8_t) * g_params.worker_count);

	memset(g_stat.worker_stat, 0, sizeof(Stat_t) * g_params.worker_count);
	memset(g_stat.worker_is_ready, 0, sizeof(uint8_t) * g_params.worker_count);

	if (g_stat.worker_stat == NULL || g_stat.worker_is_ready == NULL)
	{
		LOG_ERR("Can't allocate mamory for statistics\n");
		return ERR_MEMORY_ALLOC;
	}

	g_stat.first_sample = 1;

	pthread_mutex_init(&g_stat.mutex, NULL);

	return ERR_OK;
}

void stat_start_time_count()
{
	gettimeofday(&g_stat.start_time, NULL);
	gettimeofday(&g_stat.period_start_time, NULL);
}

void stat_print_total_stat()
{
	int i;

	pthread_mutex_lock(&g_stat.mutex);

	summarize();
	print_sample();

	fprintf(stdout, "\n%-30s : %f\n", "Test time (s)", (double)(g_stat.progress_time/1000000.0));

	for (i=0; i<COUNTER_COUNT; i++)
		fprintf(stdout, "%-30s : %lu\n", counter_name[i], g_stat.total.c[i]);

	if(g_stat.sample_count > 0)
		for (i=0; i<RATE_COUNTER_COUNT; i++)
			fprintf(stdout, "%-30s : %lu\n", rate_counter_name[i], g_stat.rate_average[i]/g_stat.sample_count);

	printf("\n");

	pthread_mutex_unlock(&g_stat.mutex);
}

Error_t stat_get_sample()
{
	Error_t rc ;

	pthread_mutex_lock(&g_stat.mutex);
	summarize();
	rc = print_sample();
	pthread_mutex_unlock(&g_stat.mutex);

	return rc;
}

void stat_print_short_report()
{
	fprintf(stdout, "%10.2f second(s) <=> %lu messages\n", (double)(g_stat.progress_time/1000000.0), g_stat.total.c[MSG_SENT]);
}

void stat_refresh_worker_stat(Stat_t* ws, uint16_t	wid, uint8_t worker_is_ready)
{
	int rc;

	g_stat.worker_is_ready[wid] = worker_is_ready;

	// To improve performance we don't wait on the lock here.
	// In the worst case we will just lose several statistics samples.

	if ((rc = pthread_mutex_trylock(&g_stat.mutex))!=0)
		return;

	memcpy(&g_stat.worker_stat[wid], ws, sizeof(Stat_t));
	pthread_mutex_unlock(&g_stat.mutex);

}

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

#ifndef _STAT_H_
#define _STAT_H_

#include "general_defs.h"

#include <sys/time.h>
#include <stdint.h>
#include <pthread.h>


typedef enum RateCounter_
{
	REQ_PER_SEC,
	BYTES_SENT_PER_SEC,
	BYTES_RCVD_PER_SEC,

	RATE_COUNTER_COUNT

} RateCounter_t;

typedef enum Counter_
{
	SOCK_OPEN,
	SOCK_CLOSE,
	EPOLL_ERR,
	SOCK_ERR,
	SOCK_HUP,
	BYTES_SENT,
	BYTES_RCVD,
	MSG_SENT,
	MSG_RCVD,

	COUNTER_COUNT

} Counter_t;

typedef struct Stat_
{
	uint64_t		c[COUNTER_COUNT];

} Stat_t;

typedef struct GlobalStat_
{
	Stat_t			total;

	struct timeval	start_time;
	struct timeval	period_start_time;

	Stat_t*			worker_stat;      /* copies of worker's statistics */
	uint8_t*		worker_is_ready;  /* a flag that indicates that stet-by-step load increase is finished and average statistics can be calculated. */

	uint64_t		progress_time;    /* milliseconds */
	uint64_t		rate_average[RATE_COUNTER_COUNT];
	uint64_t		rate_period[RATE_COUNTER_COUNT];
	uint32_t		sample_count;
	uint8_t			worker_count;
	uint8_t			first_sample;

	pthread_mutex_t	mutex;

} GlobalStat_t;

Error_t stat_init();
void stat_start_time_count();
void stat_print_total_stat();
Error_t stat_get_sample();
void stat_print_short_report();

/*
 This function copies worker stat data from local worker structure to per-worker structures
 in global statistics. We forbid direct access from the worker code to the global per-worker
 statistics in order to limit mutex usage which is expensive performance-wise.
 */
void stat_refresh_worker_stat(Stat_t* ws, uint16_t wid, uint8_t worker_is_ready);

#endif /* _STAT_H_ */

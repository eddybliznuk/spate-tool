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

#ifndef _PARAMS_H_
#define _PARAMS_H_


#include "general_defs.h"
#include "stat.h"
#include "parser.h"

#define	MAX_CONFIG_PARAM_VAL	2

/* States for config file parser */
typedef enum ConfigState_
{
	CONFIG_PARAM_SPACES,
	CONFIG_PARAM_NAME,
	CONFIG_PARAM_VAL,
	CONFIG_COMMENT

} ConfigState_e;


typedef enum SpateMode_
{
	SPATE_MODE_CLIENT = 0,
	SPATE_MODE_SERVER

} SpateMode_e;

typedef struct ConfigParam_
{
	const char* name;
	int			val_count;

} ConfigParam_t;

typedef enum ConfigParam__
{
	CONFIG_UNKNOWN,

	CONFIG_MODE,
	CONFIG_LISTENER,
	CONFIG_TUPLE,
	CONFIG_WORKERS,
	CONFIG_CONNECTIONS,
	CONFIG_REQUESTS,
	CONFIG_DELAY,
	CONFIG_REQ_PER_CONN,
	CONFIG_STEPS,
	CONFIG_REQ_BODY_SIZE,
	CONFIG_REQ_METHOD,
	CONFIG_RESP_BODY_SIZE,
	CONFIG_READ_BUFFER_SIZE,
	CONFIG_VERBOSE,
	CONFIG_SAMPLE_PERIOD,
	CONFIG_SAMPLE_FILE_PATH,
	CONFIG_AFFINITY,
	CONFIG_WORKER_CLEANUP_TIMEOUT,
	CONFIG_EPOLL_TIMEOUT,
	CONFIG_EPOLL_MAX_EVENTS,
	CONFIG_PARSE_EVERY_HTTP,

	CONFIG_PARAM_COUNT

} ConfigParam_e;

typedef struct Params_
{
	uint64_t	 	req_count;
	uint32_t 		conn_count;
	uint32_t 		worker_count;
	uint32_t		sample_period;
	uint32_t		delay;
	uint32_t		req_per_conn;
	uint16_t		start_steps;
	uint16_t		stop_steps;  				/* TODO Implement step-by-step connection closing */
	uint32_t		step_time;
	uint32_t		req_size;
	uint32_t		resp_size;
	uint32_t		read_buff_size;
	uint32_t		worker_cleanup_timeout;
	uint32_t		epoll_timeout;  			/* milliseconds */
	uint32_t		epoll_max_events;
	uint8_t			req_method;
	uint8_t			parse_every_http;
	uint8_t			verbose;
	uint8_t			mode;
	uint32_t		accept_q_size;

	char			sample_path[MAX_PARAM_STR_LEN];
	char			config_path[MAX_PARAM_STR_LEN];
	char			target[MAX_PARAM_STR_LEN];

	TupleList_t*	tuple_list;
	uint32_t		tuple_count;

	TupleList_t*	listener_list;
	uint32_t		listener_count;

	unsigned long	affinity_list[MAX_THREAD_COUNT];  /* List of affinity CPU masks */

} Params_t;

extern Params_t g_params;

void 		params_init();
Error_t 	params_parse(int argc, char **argv);

#endif /* _PARAMS_H_ */

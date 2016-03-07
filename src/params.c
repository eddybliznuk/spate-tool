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

#include "params.h"

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "utils.h"


Params_t g_params = {0};

/* First element - parameter name, second one - number of values divided by colon */
static ConfigParam_t config_param[] = {
	{"Unknown", 			0},
	{"Mode", 				1},
	{"Listener",			1},
	{"Tuple", 				2},
	{"Workers", 			1},
	{"Connections",			1},
	{"Requests",			1},
	{"Delay", 				1},
	{"ReqPerConnection",	1},
	{"Steps", 				2},
	{"ReqBodySize",			1},
	{"ReqMethod",			1},
	{"RespBodySize",		1},
	{"ReadBufferSize",		1},
	{"VerboseLevel"	,		1},
	{"SamplePeriod",		1},
	{"SampleFilePath",		1},
	{"WorkerAffinity",		1},
	{"WorkerCleanupTimeout",1},
	{"EpollTimeout",		1},
	{"EpollMaxEvents",		1},
	{"ParseEveryHttp",		1}
};

static const char* option_string="c:w:s:r:t:d:o:p:vhV";

static struct option long_options[] = {
    {"connections",	required_argument,	0,  'c' },
    {"workers",  	required_argument,	0,  'w' },
    {"requests",  	required_argument,	0,  'r' },
    {"target",  	required_argument,	0,  't' },
    {"steps",  		required_argument,	0,  's' },
    {"delay",  		required_argument,	0,  'd' },
    {"config", 		required_argument,	0,  'o' },
    {"req-body", 	required_argument,	0,  'b' },
    {"verbose",		no_argument,		0,  'v' },
    {"help",		no_argument,		0,  'h' },
    {"version",		no_argument,		0,  'V' },
    {0,             0,                  0,  0   }
};

/****************************************************************
 *
 *                   Private Functions
 *
 ****************************************************************/

static void print_help()
{
	fprintf(stdout,
	"spate - HTTP performance test tool. Copyright (c) 2014 Eddie Blizniuk (Ed Blake)\n\n"
	"Usage:  spate [options]\n\n"
	"Options:\n"
    "    -b, --req-body      request HTTP body size in bytes. Default value : 0\n"
	"    -c, --connections   number of HTTP connections. Default value: 1\n"
    "    -d, --delay         delay between reception a reply and sending the next request on a connection. \n"
    "                        Default value : 0\n"
    "    -h, --help          print this help\n"
    "    -o, --config        path to configuration file. Default value : '/etc/spate.cfg'\n"
	"    -r, --requests      total number or requests to be sent to the server(s). Default value :1\n"
	"    -s, --steps         upon the start increase the number of connection gradually, step-by-step. \n"
	"                        Format of this parameter./spate_config.cfg: -s<time>,<count>; where 'time' is duration of one step in seconds\n"
	"                        'count' - number of the steps. Number of connections added per step is : <connections>/<steps>\n"
    "    -t, --target        server URL.\n"""
	"    -v, --verbose       print short statistics every second\n"
	"    -V, --version       print version and exit\n"
    "    -w, --workers       number of worker processes. Default value :1\n"
	);
}

static void print_version()
{
	fprintf(stdout, "spate version %s\n", version);
}


static Error_t parse_steps(char* p)
{
	char* 	token;
	char  	delim = ',';
	int		i=0;

	while((token = strtok(p, &delim)) != NULL)
	{
		p = NULL;
		switch (i++)
		{
		case 0:
			g_params.step_time = atoi(token)*1000000;
			break;
		case 1:
			g_params.start_steps = atoi(token);
			break;
		case 2:
			g_params.stop_steps = atoi(token);
			break;
		default:
			return ERR_INVALID_STEP_FORMAT;
		}
	}

	if(g_params.start_steps == 0)
		g_params.start_steps = 1;

	if(g_params.stop_steps == 0)
		g_params.stop_steps = 1;

	return ERR_OK;
}

static char* strstrip(char *s)
{
    size_t size;
    char *end;

    if(s == NULL)
    	return NULL;

    size = strlen(s);

    if (!size)
    	return s;

    end = s + size - 1;
    while (end >= s && *end == ' ')
    	end--;
    *(end + 1) = '\0';

    while (*s && *s == ' ')
    	s++;

    return s;
}

static Error_t add_listener (char* local)
{
	Error_t rc = ERR_OK;
	Tuple_t*  t = malloc(sizeof(Tuple_t));

	memset (t, 0, sizeof(Tuple_t));
	local = strstrip(local);

	Url_t url;

	if((rc = parser_parse_url(local, &url)) != ERR_OK)
	{
		LOG_ERR("Configuration error: invalid listener address or port [%s]\n", local);
		free(t);
		return rc;
	}

	t->local_addr.sin_family = AF_INET;
	t->local_addr.sin_addr.s_addr = 0;  // TODO use IP
	t->local_addr.sin_port = htons(url.port);

	TupleList_t* tl = malloc(sizeof(TupleList_t));

	tl->t = t;
	tl->next = g_params.listener_list;
	g_params.listener_list = tl;
	++g_params.listener_count;

	return rc;
}

static Error_t add_tuple (char* local, char* remote)
{
	Error_t rc = ERR_OK;

	local = strstrip(local);
	remote = strstrip(remote);

	Tuple_t*  t = malloc(sizeof(Tuple_t));

	memset (t, 0, sizeof(Tuple_t));

	if(local != NULL && (rc = utils_host_to_sockaddr (local, (struct sockaddr*)&t->local_addr)) != ERR_OK)
	{
		LOG_ERR("Configuration error: can't resolve local address [%s]\n", local);
		goto add_tuple_err;
	}

	if((rc = parser_parse_url(remote, &t->url)) != ERR_OK)
	{
		LOG_ERR("Configuration error: invalid remote URL format [%s]\n", remote);
		goto add_tuple_err;
	}

	if((rc = utils_host_to_sockaddr (t->url.host,  (struct sockaddr*)&t->remote_addr)) != ERR_OK)
	{
		LOG_ERR("Configuration error: can't resolve remote address [%s]\n", remote);
		goto add_tuple_err;
	}

	t->remote_addr.sin_port = htons(t->url.port);

	TupleList_t* tl = malloc(sizeof(TupleList_t));

	tl->t = t;
	tl->next = g_params.tuple_list;
	g_params.tuple_list = tl;
	++g_params.tuple_count;

	return rc;

add_tuple_err:
	free(t);
	return rc;
}

static HttpMethod_e get_http_method (char* str)
{
	if (strcmp(str, "GET") == 0)
		return HTTP_METHOD_GET;
	else if (strcmp(str, "POST") == 0)
		return HTTP_METHOD_POST;
	else if (strcmp(str, "PUT") == 0)
		return HTTP_METHOD_PUT;
	else if (strcmp(str, "HEAD") == 0)
		return HTTP_METHOD_HEAD;

	return HTTP_METHOD_UNKNOWN;
}



static Error_t assign_config_param (ConfigParam_e idx, char* val1, char* val2)
{
	Error_t rc = ERR_OK;

	switch(idx)
	{
	case CONFIG_MODE:
		if(strstr(val1, "server"))
			g_params.mode = SPATE_MODE_SERVER;
		break;
	case CONFIG_LISTENER:
		rc = add_listener (val1);
		break;
	case CONFIG_TUPLE:
		/* Command line '-t' param prevails over 'Tuple' param */
		if (strlen(g_params.target) == 0)
			rc = add_tuple (val1, val2);
		break;
	case CONFIG_WORKERS:
		g_params.worker_count = atoi(val1);
		break;
	case CONFIG_CONNECTIONS:
		g_params.conn_count = atoi(val1);
		break;
	case CONFIG_REQUESTS:
		g_params.req_count = strtoul(val1, NULL, 10);
		break;
	case CONFIG_DELAY:
		g_params.delay = atoi(val1)*1000; /* Delay in params is in microseconds, in configuration in milliseconds */
		break;
		break;
	case CONFIG_REQ_PER_CONN:
		g_params.req_per_conn = atoi(val1);
		break;
	case CONFIG_STEPS:
		g_params.step_time = atoi(val1)*1000000;
		g_params.start_steps = atoi(val2);
		break;
	case CONFIG_REQ_BODY_SIZE:
		g_params.req_size = atoi(val1);
		break;
	case CONFIG_REQ_METHOD:
		val1 = strstrip(val1);
		g_params.req_method = get_http_method(val1);
		if (g_params.req_method == HTTP_METHOD_UNKNOWN)
		{
			LOG_ERR("Unrecognized HTTP method in configuration file [%s].\n", val1);
			return ERR_INVALID_CONFIG;
		}
		break;
	case CONFIG_RESP_BODY_SIZE:
		g_params.resp_size = atoi(val1);
		break;
	case CONFIG_READ_BUFFER_SIZE:
		g_params.read_buff_size = atoi(val1);
		break;
	case CONFIG_VERBOSE:
		g_params.verbose = atoi(val1);
		break;
	case CONFIG_SAMPLE_PERIOD:
		g_params.sample_period = atoi(val1)*1000;  /* in microseconds */
		break;
	case CONFIG_SAMPLE_FILE_PATH:
		strncpy(g_params.sample_path, strstrip(val1), sizeof(g_params.sample_path));
		break;
	case CONFIG_AFFINITY:
		rc = parser_parse_affinity(val1, g_params.affinity_list, MAX_THREAD_COUNT);
		break;
	case CONFIG_WORKER_CLEANUP_TIMEOUT:
		g_params.worker_cleanup_timeout = atoi(val1)*1000000;
		break;
	case CONFIG_EPOLL_TIMEOUT:
		g_params.epoll_timeout = atoi(val1);
		break;
	case CONFIG_EPOLL_MAX_EVENTS:
		g_params.epoll_max_events = atoi(val1);
		break;
	case CONFIG_PARSE_EVERY_HTTP:
		g_params.parse_every_http = atoi(val1);
		break;
	default:
		// Unknown param - do nothing
		return ERR_OK;
	}

	return rc;
}

static Error_t parse_config(uint8_t default_file)
{
	FILE* f;
	ConfigState_e	state = CONFIG_PARAM_SPACES;

	f = fopen (g_params.config_path, "r");

	if (f == NULL)
	{
		if(default_file)
		{
			/* If there is no config file use all defaults */
			return ERR_OK;
		}
		else
		{
			LOG_ERR("Can't open configuration file [%s]\n", g_params.config_path);
			return ERR_CANT_OPEN_FILE;
		}
	}


	char 			param_name[MAX_PARAM_STR_LEN];
	char 			param_val[MAX_CONFIG_PARAM_VAL][MAX_PARAM_STR_LEN];
	char* 			name = param_name;
	char*			val = &param_val[0][0];

	int				param_count = sizeof(config_param)/sizeof(ConfigParam_t);
	ConfigParam_e	param_idx = CONFIG_UNKNOWN;
	int				cur_val_index = 0;
	int 			cur_char;

	cur_char = getc(f);

	while (cur_char != EOF)
	{
		char c = (char)cur_char;

		switch (state)
		{
		case CONFIG_PARAM_SPACES:

			if(c == ' ' || c == '\n' || c == '\r')
			{
				break;
			}
			else if (c == '#')
			{
				state = CONFIG_COMMENT;
			}
			else
			{
				name = param_name;
				*name++ = c;
				state = CONFIG_PARAM_NAME;
			}

			break;

		case CONFIG_COMMENT:
			if(c == '\n')
			{
				state = CONFIG_PARAM_SPACES;
			}
			break;

		case CONFIG_PARAM_NAME:
			if(c == '=')
			{
				*name = 0;
				name = param_name;
				param_idx = CONFIG_UNKNOWN;

				for(int i=0; i<param_count; ++i)
				{
					if (strstr(param_name, config_param[i].name) != NULL)
					{
						param_idx = i;
						break;
					}
				}

				if (param_idx == CONFIG_UNKNOWN)
					LOG_WARN("Unknown parameter [%s] in the configuration file - ignored.\n", param_name);

				state = CONFIG_PARAM_VAL;
				val = &param_val[0][0];
				cur_val_index = 0;
			}
			else if (c != ' ')
			{
				*name++ = c;
			}
			break;

		case CONFIG_PARAM_VAL:

			if(c == '\n')
			{
				*val = 0;

				if (assign_config_param(param_idx, &param_val[0][0],  &param_val[1][0]) != ERR_OK)
				{
					fclose(f);
					return ERR_INVALID_CONFIG;
				}

				state = CONFIG_PARAM_SPACES;

			}
			else if(c == ',')
			{
				if (val != &param_val[0][0] &&
					cur_val_index < config_param[param_idx].val_count)
				{
					*val = 0;
					val=&param_val[++cur_val_index][0];
				}

				state 	= CONFIG_PARAM_VAL;
			}
			else
				*val++ = c;

			break;

		}

		cur_char = getc(f);
	}

	fclose(f);
	return ERR_OK;
}

static Error_t verify_params()
{
	/* Server/client mode specific params */
	if(g_params.mode == SPATE_MODE_SERVER)
	{
		if (g_params.listener_count == 0)
		{
			LOG_ERR("No listeners specified in server mode.\n");
			return ERR_INVALID_CONFIG;
		}
	}
	else
	{
		if (g_params.tuple_list == NULL)
		{
			LOG_ERR("No remote targets specified. Please use '-t' command line option or define at least one tuple in the configuration file.\n");
			return ERR_INVALID_CONFIG;
		}

		if (g_params.conn_count < g_params.worker_count)
		{
			LOG_ERR("Connection count can't be less then worker count\n");
			return ERR_INVALID_CONFIG;
		}

		if (g_params.req_count < g_params.conn_count)
		{
			LOG_ERR("Request count can'y be less then connection count\n");
			return ERR_INVALID_CONFIG;
		}
	}

	if (g_params.worker_count == 0)
		g_params.worker_count = 1;

	if(g_params.read_buff_size == 0)
		g_params.read_buff_size = (1024 * 10);

	if(g_params.start_steps == 0)
		g_params.start_steps = 1;

	if(g_params.sample_period >0 && g_params.sample_period < 100000)
	{
		LOG_ERR("Minimal sample period is 100 ms\n");
		return ERR_INVALID_CONFIG;
	}

	if(g_params.worker_count > MAX_THREAD_COUNT)
	{
		LOG_ERR("Worker count exceeds maximum [%d]\n", MAX_THREAD_COUNT);
		return ERR_INVALID_CONFIG;
	}

	return ERR_OK;
}

static void print_config()
{
	if(g_params.mode == SPATE_MODE_SERVER)
	{
		printf ("Starting spate in SERVER mode with parameters:\n"
			"listener count             = %u\n"
			"worker count               = %u\n"
			"connection count           = %u\n"
			"request count              = %lu\n"
			"delay (ms)                 = %u\n"
			"req per connection         = %u\n"
			"resp body size             = %u\n"
			"read buffer size           = %u\n"
			"verbose                    = %u\n"
			"sample period (ms)         = %u\n"
			"worker cleanup timeout (s) = %u\n"
			"epoll timeout (ms)         = %u\n"
			"epoll max events           = %u\n"
			"parse every http           = %u\n\n",
			g_params.listener_count,
			g_params.worker_count,
			g_params.conn_count,
			g_params.req_count,
			g_params.delay/1000,
			g_params.req_per_conn,
			g_params.resp_size,
			g_params.read_buff_size,
			g_params.verbose,
			g_params.sample_period/1000,
			g_params.worker_cleanup_timeout/1000000,
			g_params.epoll_timeout,
			g_params.epoll_max_events,
			g_params.parse_every_http);
	}
	else
	{
		printf ("Starting spate in CLIENT mode with parameters:\n"
			"worker count               = %u\n"
			"connection count           = %u\n"
			"request count              = %lu\n"
			"delay (ms)                 = %u\n"
			"req per connection         = %u\n"
			"step_time (s)              = %u\n"
			"start_steps                = %u\n"
			"req body size              = %u\n"
			"req method                 = %s\n"
			"read buffer size           = %u\n"
			"verbose                    = %u\n"
			"sample period (ms)         = %u\n"
			"worker cleanup timeout (s) = %u\n"
			"epoll timeout (ms)         = %u\n"
			"epoll max events           = %u\n"
			"parse every http           = %u\n\n",
			g_params.worker_count,
			g_params.conn_count,
			g_params.req_count,
			g_params.delay/1000,
			g_params.req_per_conn,
			g_params.step_time/1000000,
			g_params.start_steps,
			g_params.req_size,
			utils_get_http_method_name(g_params.req_method),
			g_params.read_buff_size,
			g_params.verbose,
			g_params.sample_period/1000,
			g_params.worker_cleanup_timeout/1000000,
			g_params.epoll_timeout,
			g_params.epoll_max_events,
			g_params.parse_every_http);
	}
}
/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/

void params_init ()
{
	memset(&g_params, 0, sizeof(Params_t));

	g_params.mode 					= SPATE_MODE_CLIENT;

	g_params.worker_count 			= 1;
	g_params.conn_count 			= 1;
	g_params.req_count 				= 1;
	g_params.delay	 				= 0;
	g_params.req_per_conn			= 0; /* infinite, don't close connection until test finishes */

	g_params.start_steps			= 1;
	g_params.stop_steps				= 1;
	g_params.step_time				= 0;

	g_params.req_size				= 0;
	g_params.req_method				= HTTP_METHOD_GET;
	g_params.read_buff_size			= (1024 * 10);
	g_params.verbose				= 0;
	g_params.sample_period			= 1000000;  /* 1 second */
	strcpy (g_params.sample_path, "spate-stat.txt");

	g_params.worker_cleanup_timeout	= 10000000;
	g_params.epoll_timeout			= 10;
	g_params.epoll_max_events		= 1000;
	g_params.parse_every_http		= 0;

	g_params.target[0]				= 0;
	g_params.accept_q_size			= 1000;  // TODO Make configurable

	strcpy (g_params.config_path, "/etc/spate.cfg");
}

Error_t	 params_parse (int argc, char **argv)
{
	Error_t		rc;
	int 		c;
	int 		option_index = 0;
	uint8_t 	default_file = 1;

	while ((c = getopt_long(argc, argv, option_string, long_options, &option_index)) != -1)
	{
		switch (c)
		{
		case 'x':
			default_file = 0;
			strncpy(g_params.config_path, optarg, sizeof(g_params.config_path));
			break;
        case 'h':
        	print_help();
        	exit(0);
		case 't':
			strncpy(g_params.target, optarg, sizeof(g_params.target));
			add_tuple (NULL, g_params.target);
			break;
        case 'V':
        	print_version();
        	exit(0);
		}
    } /* while */

	if ((rc = parse_config(default_file)) != ERR_OK)
		return ERR_INVALID_CONFIG;

	optind = 1; /* reset command line options parser and start parsing again */

	while ((c = getopt_long(argc, argv, option_string, long_options, &option_index)) != -1)
	{
		switch (c)
		{
		case 'c':
        	g_params.conn_count = atoi(optarg);
        	break;
		case 'w':
			g_params.worker_count = atoi(optarg);
			break;
		case 'r':
			g_params.req_count = atoi(optarg);
			break;
		case 't':
			strncpy(g_params.target, optarg, sizeof(g_params.target));
			break;
		case 's':
			if((rc = parse_steps(optarg)) != ERR_OK)
				return rc;
			break;
		case 'd':
			// Delay is kept in microseconds
			g_params.delay = atoi(optarg)*1000000;
			break;
		case 'b':
			g_params.req_size = atoi(optarg);
			break;
		case 'v':
			g_params.verbose = 1;
			break;
		case 'o':
			default_file = 0;
			strncpy(g_params.config_path, optarg, sizeof(g_params.config_path));
			break;
        default:
        	printf("Unknown argument -%c\n", c);
        	return ERR_INVALID_CONFIG;
		}
    } /* while */


	if ((rc = verify_params()) != ERR_OK)
		return ERR_INVALID_CONFIG;

	if (g_params.verbose >= 1)
		print_config();

	return ERR_OK;
}






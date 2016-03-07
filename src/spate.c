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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>

#include "params.h"
#include "dispatcher.h"
#include "stat.h"
#include "log.h"

#ifdef _UTEST
	#include "utest.h"
#endif

const char* version="2.0.0";

static void sig_handler(int signo)
{
  if (signo == SIGINT)
  {
	  	printf("User Interrupt\n\n");
	  	dispatcher_close_all();
	  	sleep(1);
		exit(0);
  }
}

int main (int argc, char **argv)
{
#ifdef _UTEST
	utest_test_all();
	return EXIT_SUCCESS;
#endif

	Error_t rc;

	log_init();

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		return EXIT_FAILURE;

	params_init();

	if ((rc = params_parse(argc, argv)) != ERR_OK)
	{
		LOG_ERR("Invalid configuration. Exiting...\n");
		return EXIT_FAILURE;
	}

	stat_init();

	if ((rc = dispatcher_init_and_start_workers()) != ERR_OK)
	{
		LOG_ERR("Fatal dispatcher error. Exiting...\n");
		return EXIT_FAILURE;
	}

	time_t verbose_clock = time(NULL);

	while(1)
	{
		if(g_params.sample_period > 0)
		{
			usleep(g_params.sample_period);
			stat_get_sample();
		}
		else
			usleep(1000000);

		if(g_params.verbose >= 1 && (time(NULL) > verbose_clock))
		{
			verbose_clock = time(NULL);
			stat_print_short_report();
		}
	}

	return EXIT_SUCCESS;
}

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

#include "utils.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>


void utils_timeval_inc (struct timeval* tv, uint32_t inc)
{
	tv->tv_usec += inc;

	if(tv->tv_usec > 1000000)
	{
		tv->tv_sec += tv->tv_usec/1000000;
		tv->tv_usec %= 1000000;
	}
}

int64_t utils_timeval_diff (struct timeval* tv1, struct timeval* tv2)
{
	int64_t val;

	val = (tv1->tv_sec - tv2->tv_sec) * 1000000;
	val += tv1->tv_usec - tv2->tv_usec;

	return val;
}

Error_t utils_host_to_sockaddr (char* host, struct sockaddr* addr)
{
	struct addrinfo*	a = NULL;
	int 				err;

	if ((err = getaddrinfo (host, NULL, NULL, &a)) != 0)
		return ERR_CANT_RESOLVE_HOST;

	if(addr != NULL)
		memcpy( addr, a->ai_addr, a->ai_addrlen);

	return ERR_OK;
}

Error_t utils_sockaddr_to_string(struct sockaddr* s, char* str, uint32_t size)
{
	struct sockaddr_in* sin = (struct sockaddr_in*)s;
	char ip_str[IP_ADDR_STRING_LEN];

	inet_ntop(sin->sin_family, &(sin->sin_addr), ip_str, IP_ADDR_STRING_LEN);

	if(sin->sin_port > 0)
		snprintf(str, size, "%s:%d", ip_str, ntohs(sin->sin_port));
	else
		snprintf(str, size, "%s", ip_str);

	return ERR_OK;
}



char*	utils_get_current_time()
{
	static char buf[MAX_TIMESTAMP_SIZE];

	time_t	now = time(NULL);

	strftime(buf, MAX_TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", localtime(&now));

	return buf;
}

void utils_eggbasket_init(EggBasket_t* d, uint32_t eggs, uint32_t baskets)
{
	d->eggs_per_basket	= eggs/baskets;
	d->additions		= eggs%baskets;
}

uint32_t utils_eggbasket_get (EggBasket_t* d)
{
	if(d->additions > 0)
	{
		--d->additions;
		return d->eggs_per_basket+1;
	}
	else
		return d->eggs_per_basket;
}


char* utils_get_http_method_name (HttpMethod_e method)
{
	switch (method)
	{
	case HTTP_METHOD_GET:
		return "GET";
	case HTTP_METHOD_POST:
		return "POST";
	case HTTP_METHOD_PUT:
		return "PUT";
	case HTTP_METHOD_HEAD:
		return "HEAD";
	default:
		break;
	}

	return "UNKNOWN";
}


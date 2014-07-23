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

#ifndef _GENERAL_DEFS_H_
#define _GENERAL_DEFS_H_

#include <stdint.h>
#include <netinet/in.h>

#define BAD_SOCKET			-1
#define DEFAULT_PORT		80
#define LOOP_GUARD 			1000
#define MAX_PARAM_STR_LEN	1024
#define IP_ADDR_STRING_LEN	64
#define MAX_WORKER_COUNT	256

extern char* version;

typedef enum Error_
{
	ERR_OK = 0,
	ERR_INVALID_STEP_FORMAT,
	ERR_CANT_CREATE_SOCKET,
	CANT_CONNECT_SOCKET,
	ERR_CANT_CREATE_EPOLL,
	CANT_ADD_TO_EPOLL,
	ERR_INVALID_SERVER_IP,
	ERR_CANT_CREATE_THREAD,
	ERR_MEMORY_ALLOC,
	ERR_OUT_OF_BOUNDS,
	ERR_CANT_OPEN_FILE,
	ERR_CANT_RESOLVE_HOST,
	ERR_INVALID_URL,
	ERR_INVALID_CONFIG,
	ERR_GENERIC = 1000

} Error_t;

typedef enum HttpMethod_
{
	HTTP_METHOD_UNKNOWN,
	HTTP_METHOD_GET,
	HTTP_METHOD_POST,
	HTTP_METHOD_PUT,
	HTTP_METHOD_HEAD

} HttpMethod_e;

typedef struct Url_
{
	char 		protocol[MAX_PARAM_STR_LEN];
	char 		host[MAX_PARAM_STR_LEN];
	char 		path[MAX_PARAM_STR_LEN];
	uint16_t	port;

} Url_t;

typedef struct Tuple_
{
	struct sockaddr_in		local_addr;
	struct sockaddr_in		remote_addr;
	char*					req_buf;
	struct Tuple_*			next;

	Url_t					url;

} Tuple_t;

typedef struct TupleList_
{
	Tuple_t*			t;
	struct TupleList_*	next;

} TupleList_t;

#endif /* _GENERAL_DEFS_H_ */

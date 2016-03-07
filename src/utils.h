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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <sys/time.h>
#include <netinet/in.h>

#include "general_defs.h"

#define MAX_TIMESTAMP_SIZE	64


typedef struct EggBasket_
{
	uint32_t eggs_per_basket;
	uint32_t additions;

} EggBasket_t;


typedef struct HashEntry_
{
	void*				ptr;
	struct HashEntry_*	next;

} HashEntry_t;


typedef struct Hash_
{
	HashEntry_t**	bucket;
	uint32_t 		size;

} Hash_t;

/*
 * utils_timeval_inc() adds to timeval value 'inc' microseconds.
 */
void 	utils_timeval_inc (struct timeval* tv, uint32_t inc);

/*
 * utils_timeval_diff() calculates differences between two timeval values in microseconds.
 */
int64_t	utils_timeval_diff (struct timeval* tv1, struct timeval* tv2);

/*
 * utils_host_to_sockaddr() resolves host name using DNS and fills up sockaddr structure.
 * Not tested with IPv6.
 */
Error_t utils_host_to_sockaddr (char* host, struct sockaddr* addr);

/*
 * utils_sockaddr_to_string() shows string representation of sockaddr as <ip>:<port>. Not tested with IPv6.
 */
Error_t utils_sockaddr_to_string(struct sockaddr* s, char* str, uint32_t size);

/*
 * utils_get_current_time() retrieves current date/time stamp wit seconds resolution as a string
 */
char*	utils_get_current_time();

/*
 The EggBasket helper distributes N 'eggs'to M 'baskets' trying to do it as evenly as possible
 For example, if we want to distribute 14 connections between 10 worker threads
 it assigns 2 connections to first 4 threads and 1 connection to the rest.
 */
void 		utils_eggbasket_init(EggBasket_t* d, uint32_t eggs, uint32_t baskets);
uint32_t	utils_eggbasket_get (EggBasket_t* d);

/* This function is NOT thread safe. */
char* utils_get_http_method_name (HttpMethod_e method);


/* Simple hash of poiters */
Error_t utils_hash_init(Hash_t* h, uint32_t size);
Error_t utils_hash_add(Hash_t* h, void* ptr);
void utils_hash_del(Hash_t* h, void* ptr);
void* utils_hash_enum(Hash_t* h, uint8_t start);


#endif /* _UTILS_H_ */

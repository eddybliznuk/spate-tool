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

#ifndef _PARSER_H_
#define _PARSER_H_

#include "general_defs.h"

#define MAX_HEADER_NAME_SIZE		120 /* The size of this param used to optimize structure size to cache line size */
#define MAX_HEADER_VAL_SIZE			32  /* Yes, we don't need to keep long headers here */
#define MAX_AFFINITY_STR_LEN		128

typedef enum HttpState_
{
	HTTP_START_LINE,
	HTTP_HEADER_NAME,
	HTTP_HEADER_VAL,
	HTTP_BODY

} HttpState_t;

typedef enum UrlState_
{
	URL_START = 1,
	URL_PROTO_OR_PORT_1,
	URL_PROTO_OR_PORT_2,
	URL_HOST,
	URL_PORT,
	URL_PATH

} UrlState_t;

typedef enum AffinitySate_
{
	AFF_SPACES = 1,
	AFF_VAL

} AffinityState_t;


typedef struct Parser_
{
	uint32_t 	content_len;
	uint32_t	reply_len;
	char*		hn_pos;
	char*		hv_pos;
	uint8_t		chunked;
	uint8_t		state;
	uint16_t	header_len;

	char		header_name[MAX_HEADER_NAME_SIZE+1];
	char		header_val[MAX_HEADER_VAL_SIZE+1];

} Parser_t;


Error_t parser_parse_http(Parser_t* par, char* buf, int size, uint8_t start);
Error_t parser_parse_url(char* src, Url_t* url);
Error_t parser_parse_affinity(char* str, unsigned long* list, uint32_t size);

#endif /* _PARSER_H_ */

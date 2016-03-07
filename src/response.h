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

#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include "general_defs.h"

#define MAX_RESP_HEADER_SIZE	1024
#define MAX_HTTP_PARAM_LEN			128

#define RESP_STATUS_LINE		"HTTP 1.1 200 OK\r\n"
#define RESP_CONTENT_TYPE		"Content-Type: text/xml; charset=utf-8\r\n"
/*
 * Allocates memory for HTTP response and fills it up with HTTP headers and body.
 * If body is defined it is filled with A character.
 */
Error_t response_create_response(char** buff);


#endif /* _RESPONSE_H_ */

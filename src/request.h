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

#ifndef _REQUEST_H_
#define _REQUEST_H_

#include "general_defs.h"

#define MAX_REQ_HEADER_SIZE	1024
#define MAX_HTTP_PARAM_LEN	128

/*
 * Allocates memory for HTTP requests and fills it up with HTTP headers and body.
 * If body is defined it is filled with A character.
 */
Error_t request_create_request(char** req_buf, Url_t* url);


#endif /* _REQUEST_H_ */

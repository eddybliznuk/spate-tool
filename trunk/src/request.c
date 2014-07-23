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

#include "request.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "params.h"
#include "utils.h"


Error_t request_create_request(char** req_buf, Url_t* url)
{
	uint32_t buf_len = g_params.req_size + MAX_REQ_HEADER_SIZE;
	char tmp_buf[MAX_REQ_HEADER_SIZE];

	*req_buf = (char*)malloc(buf_len);

	if(*req_buf == NULL)
		return ERR_MEMORY_ALLOC;

	char* method_name = utils_get_http_method_name (g_params.req_method);

	strcat(*req_buf, method_name);
	strcat(*req_buf, " ");
	buf_len -= strlen(method_name+1);

	if(strlen(url->path) != 0 && (strlen(url->path) < buf_len))
	{
		strncat(*req_buf, url->path, buf_len);
		buf_len -= strlen(url->path);
	}
	else
	{
		strncat(*req_buf, "/", buf_len);
		--buf_len;
	}

	snprintf(tmp_buf, sizeof(tmp_buf), " HTTP/1.1\r\nUser-Agent: spate %s\r\nHost: %s\r\n", version, url->host);
	strncat(*req_buf, tmp_buf, buf_len);
	buf_len -= strlen(tmp_buf);

	if(g_params.req_size > 0)
	{
		snprintf(tmp_buf, sizeof(tmp_buf), "Content-Length: %d\r\n", g_params.req_size);
		strncat(*req_buf, tmp_buf, buf_len);
		buf_len -= strlen(tmp_buf);
	}

	strncat(*req_buf, "\r\n", buf_len);
	buf_len -= 2;

	uint32_t i=0;

	while (i<g_params.req_size && buf_len>1)
	{
		strcat(*req_buf, "A");
		++i;
	}

	return ERR_OK;
}

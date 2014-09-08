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

#include "response.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "params.h"
#include "utils.h"


Error_t response_create_response(char** buff)
{
	uint32_t buff_len = g_params.resp_size + MAX_RESP_HEADER_SIZE;
	char tmp_buff[MAX_RESP_HEADER_SIZE];

	*buff = (char*)malloc(buff_len);

	if(*buff == NULL)
		return ERR_MEMORY_ALLOC;

	strncat(*buff, RESP_STATUS_LINE, buff_len);
	buff_len -= strlen(RESP_STATUS_LINE+1);

	strncat(*buff, RESP_CONTENT_TYPE, buff_len);
	buff_len -= strlen(RESP_CONTENT_TYPE+1);

	if(g_params.resp_size > 0)
	{
		snprintf(tmp_buff, sizeof(tmp_buff), "Content-Length: %d\r\n", g_params.resp_size);
		strncat(*buff, tmp_buff, buff_len);
		buff_len -= strlen(tmp_buff);
	}

	strncat(*buff, "\r\n", buff_len);
	buff_len -= 2;

	uint32_t i=0;

	while (i<g_params.resp_size && buff_len>1)
	{
		strcat(*buff, "A");
		++i;
		--buff_len;
	}

	return ERR_OK;
}

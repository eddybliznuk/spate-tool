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

#include "parser.h"
#include <string.h>
#include <stdlib.h>

inline static void init_data(Parser_t* par)
{
	par->state			= HTTP_START_LINE;
	par->header_len 	= 0;
	par->content_len 	= 0;
	par->reply_len		= UINT32_MAX;
	par->chunked		= 0;

	par->header_name[0] = 0;
	par->hn_pos			= par->header_name;

	par->header_val[0] 	= 0;
	par->hv_pos			= par->header_val;

}

char token_map[128] = { 0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0, // [0 -15]   - CTRL
						0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0,   0, 0, 0, 0, // [16-31]   - CTRL
						0, 1, 0, 1,   1, 1, 0, 1,   0, 0, 1, 1,   0, 1, 1, 0, // [32-47]   -  , !, ", #,   $, %, &, ',  (, ), *, +,  ,, -, ., /
						1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 0, 0,   1, 0, 1, 0, // [48-63]   - 0, 1, 2, 3,   4, 5, 6, 7,  8, 9, :, ;,  <, =, >, ?
						0, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1, // [64-79]   - @, A, B, C,   D, E, F, G,  H, I, J, K,  L, M, N, O
						1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0,   0, 0, 1, 1, // [80-95]   - P, Q, R, S,   T, U, V, W,  X, Y, Z, [,  \, ], ^, _
						1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 1, // [96-111]  - `, a, b, c,   d, e, f, g,  h, i, j, k,  l, m, n, o
						1, 1, 1, 1,   1, 1, 1, 1,   1, 1, 1, 0,   1, 0, 1, 1, // [112-127] - p, q, r, s,   t, u, v, w,  x, y, z, {,  |, }, ~, <DEL>


};

inline static int is_token(char c)
{
	return token_map[(int)c];
}

static void process_header(Parser_t* par)
{
	// TODO Optimize header name analysis if we will parse more than 2-3 headers

	if (strstr(par->header_name, "Content-Length"))
		par->content_len = atoi(par->header_val);
}


/****************************************************************
 *
 *                   Interface Functions
 *
 ****************************************************************/


Error_t parser_parse_http(Parser_t* par, char* buf, int size, uint8_t start)
{
	char* cur = buf;
	char* end = buf + size;

	if (start)
		init_data (par);

	while (cur < end )
	{
		char c = *cur;
		++cur;

		switch(par->state)
		{
		case HTTP_START_LINE:
			if(c == '\n')
				par->state = HTTP_HEADER_NAME;

			break;

		case HTTP_HEADER_NAME:
			if (is_token(c))
			{
				*par->hn_pos = c;
				++par->hn_pos;
			}
			else if(c == ':')
			{
				// Finished to read header name, start reading header value
				par->state = HTTP_HEADER_VAL;
				*par->hn_pos=0;
				par->hn_pos = par->header_name;
			}
			else if (c == '\n' && par->hn_pos == par->header_name)
			{
				// '/n' met twice and no headers between then => end of the header state
				par->state 	= HTTP_BODY;
				par->header_len += cur-buf;
				par->reply_len 	= par->header_len + par->content_len;
			}

			break;

		case HTTP_HEADER_VAL:
			if(c == '\n')
			{
				// Finished to read header value, process this header and start reading the next one
				par->state 	= HTTP_HEADER_NAME;
				*par->hv_pos 	= 0;
				par->hv_pos		= par->header_val;

				process_header(par);
			}
			else
			{
				*par->hv_pos = c;
				++par->hv_pos;
			}

			break;

		case HTTP_BODY:
		default:
			// TODO Add chunked body parsing here
			return ERR_OK;
		}

	}

	if(par->state != HTTP_BODY)
		par->header_len = cur-buf;

	return ERR_OK;
}

Error_t parser_parse_url(char* src, Url_t* url)
{
    char	buf[MAX_PARAM_STR_LEN];
    char*	p = src;
    int		j = 0;

    UrlState_t state = URL_START;

	url->port = DEFAULT_PORT;

    for(int i=0; i<MAX_PARAM_STR_LEN; i++)
    {
    	switch (state)
    	{
    		case URL_START:
    			if (*p == ':')
    				state = URL_PROTO_OR_PORT_1;
    			else if (*p == 0)
    			{
    				// Host only
    				buf[j]=0;
    				strncpy(url->host, buf, sizeof(url->host));
    				return ERR_OK;
    			}
    			else if (*p == '/')
    			{
    				// Host and path
    				buf[j]=0;
    				strncpy(url->host, buf, sizeof(url->host));
    				j=0;
    				buf[j++]=*p;
    				state = URL_PATH;
    			}
    			else
    				buf[j++]=*p;

    			break;

    		case URL_PROTO_OR_PORT_1:
    			if (*p == '/')
    				state = URL_PROTO_OR_PORT_2;
    			else
    			{
    				buf[j]=0;
    				strncpy(url->host, buf, sizeof(url->host));
    				j=0;
       				buf[j++]=*p;
    				state = URL_PORT;
    			}
    			break;

    		case URL_PROTO_OR_PORT_2:
    			if (*p == '/')
    			{
    				buf[j]=0;
    				strncpy(url->protocol, buf, sizeof(url->host));
    				j=0;
    				state = URL_HOST;
    			}
    			else
    				return ERR_INVALID_URL;
    			break;

    		case URL_HOST:
    			if (*p == '/')
    			{
    				buf[j]=0;
    				strncpy(url->host, buf, sizeof(url->host));
    				j=0;
    				buf[j++]=*p;
    				state = URL_PATH;
    			}
    			else if (*p == ':')
    			{
    				buf[j]=0;
    				strncpy(url->host, buf, sizeof(url->host));
    				j=0;
    				state = URL_PORT;
    			}
    			else
    				buf[j++]=*p;

    			break;

    		case URL_PORT:
    			if (*p == '/')
    			{
    				buf[j] = 0;
    				url->port = atoi(buf);
    				j=0;
    				buf[j++]=*p;
    				state = URL_PATH;
    			}
    			else if (*p == 0)
    			{
    				buf[j]=0;
    				url->port = atoi(buf);
    				return ERR_OK;
    			}
    			else
    				buf[j++]=*p;

    			break;

    		case URL_PATH:
    			if (*p == 0)
    			{
    				buf[j]=0;
    				strncpy(url->path, buf, sizeof(url->path));
    				return ERR_OK;
    			}
    			else
    				buf[j++]=*p;

    		default:
    			break;

    	} /* switch-case */

    	if(*p == 0)
    		return ERR_INVALID_URL;
    	++p;

    } /* for */

    return ERR_OK;
}

Error_t parser_parse_affinity(char* str, unsigned long* list, uint32_t size)
{
	char* 			p = str;
	AffinityState_t state = AFF_SPACES;
	char			val[MAX_AFFINITY_STR_LEN];
	int				i = 0;
	int				par_no = 0;

	while(1)
	{
		if(state == AFF_SPACES)
		{
			if(*p != ' ' && *p != '\t')
			{
				state 		= AFF_VAL;
				i			= 0;
				val[i++]	= *p;
			}
		}
		else
		{
			if(*p == ' ' || *p == 0 )
			{
				uint32_t j = 0;

				--i;
				list[par_no]=0;

				while (i >= 0)
				{
					if (val[i] == '1')
						list[par_no] |= (1 << j);
					++j;
					--i;
				}

				if(++par_no == size)
					return ERR_GENERIC;

				state 	= AFF_SPACES;
			}
			else if ((*p == '0' || *p == '1') && i<MAX_AFFINITY_STR_LEN)
			{
				val[i++] = *p;
			}
		}

		if(*p == 0)
			break;

		++p;
	}

	return ERR_OK;
}


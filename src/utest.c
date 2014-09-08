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
#include "parser.h"
#include "fifo_ring.h"

#include <stdio.h>
#include <string.h>

static int test_time_utils()
{
	int err = 0;
	printf("Testing time utils ");

	struct timeval tv;

	printf(".");

	tv.tv_sec 	= 10;
	tv.tv_usec 	= 999995;
	utils_timeval_inc (&tv, 10);
	if(tv.tv_sec != 11 || tv.tv_usec != 5)
		++err;

	printf(".");

	tv.tv_sec 	= 10;
	tv.tv_usec 	= 999995;
	utils_timeval_inc (&tv, 3000010);
	if(tv.tv_sec != 14 || tv.tv_usec != 5)
		++err;

	printf(".");

	struct timeval tv1, tv2;
	int64_t dif;
	tv1.tv_sec 	= 10;
	tv1.tv_usec = 10;
	tv2.tv_sec 	= 10;
	tv2.tv_usec = 5;
	dif = utils_timeval_diff (&tv1, &tv2);
	if(dif != 5)
		++err;

	printf(".");

	tv1.tv_sec 	= 10;
	tv1.tv_usec = 500000;
	tv2.tv_sec 	= 5;
	tv2.tv_usec = 200000;
	dif = utils_timeval_diff (&tv1, &tv2);
	if(dif != 5300000)
		++err;

	printf(".");

	tv1.tv_sec 	= 10;
	tv1.tv_usec = 100000;
	tv2.tv_sec 	= 9;
	tv2.tv_usec = 200000;
	dif = utils_timeval_diff (&tv1, &tv2);
	if(dif != 900000)
		++err;

	printf(".");

	tv1.tv_sec 	= 5;
	tv1.tv_usec = 100000;
	tv2.tv_sec 	= 7;
	tv2.tv_usec = 200000;
	dif = utils_timeval_diff (&tv1, &tv2);
	if(dif != -2100000)
		++err;

	printf(" => %d errors\n", err);

	return err;
}

int test_eggbasket()
{
	int err = 0;
	printf("Testing egg basket ");
	int total = 0;

	EggBasket_t eb;

	printf(".");

	utils_eggbasket_init(&eb, 333, 10);

	for(int i=0; i<10; ++i)
	{
		uint32_t eggs = utils_eggbasket_get(&eb);
		if(i<3 && eggs != 34)
		{
			++err;
			break;
		}
		if(i>=3 && eggs != 33)
		{
			++err;
			break;
		}
		total += eggs;
	}

	if(total!=333)
		++err;

	printf(".");

	utils_eggbasket_init(&eb, 14, 10);

	for(int i=0; i<10; ++i)
	{
		uint32_t eggs = utils_eggbasket_get(&eb);
		if(i<4 && eggs != 2)
		{
			++err;
			break;
		}
		if(i>=4 && eggs != 1)
		{
			++err;
			break;
		}
	}

	printf(".");

	utils_eggbasket_init(&eb, 3, 10);

	for(int i=0; i<10; ++i)
	{
		uint32_t eggs = utils_eggbasket_get(&eb);
		if(i<3 && eggs != 1)
		{
			++err;
			break;
		}
		if(i>=3 && eggs != 0)
		{
			++err;
			break;
		}
	}

	printf(" => %d errors\n", err);

	return err;
}


int test_parse_affinity()
{
	int err = 0;
	printf("Testing affinity string parser ");

	printf(".");

	char* a1="0010  100 10101\n";
	unsigned long list[10];
	Error_t rc;

	rc = parser_parse_affinity(a1, list, 10);

	if(rc != ERR_OK)
		++err;
	else
	{
		if(list[0] != 2 || list[1] != 4 || list[2] != 21)
			++err;
	}

	printf(" => %d errors\n", err);

	return err;
}

int test_parse_url()
{
	int err = 0;
	printf("Testing affinity string parser ");

	Error_t rc;
	char src[256];
	Url_t url;

	printf(".");

	strcpy(src, "http://");
	rc = parser_parse_url(src, &url);

	if(rc == ERR_OK)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "www.abc.com");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.host,"www.abc.com") != 0 )
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "www.abc.com:1234");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.host,"www.abc.com") != 0 || url.port != 1234 )
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "www.abc.com:1234/");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.host,"www.abc.com") != 0 || url.port != 1234 || strcmp(url.path, "/") != 0)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "www.abc.com:1234/abc/def/111.html");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.host,"www.abc.com") != 0 || url.port != 1234 || strcmp(url.path, "/abc/def/111.html") != 0)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "www.abc.com/abc/def/111.html");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.host,"www.abc.com") != 0 || strcmp(url.path, "/abc/def/111.html") != 0)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "http://www.abc.com/abc/def/111.html");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.protocol,"http") != 0  || strcmp(url.host,"www.abc.com") != 0 || strcmp(url.path, "/abc/def/111.html") != 0)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "http://www.abc.com:321/abc/def/111.html");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.protocol,"http") != 0  || strcmp(url.host,"www.abc.com") != 0 || url.port != 321 || strcmp(url.path, "/abc/def/111.html") != 0)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "http://www.abc.com:321");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.protocol,"http") != 0  || strcmp(url.host,"www.abc.com") != 0 || url.port != 321)
		++err;

	printf(".");

	memset(&url, 0, sizeof(url));
	strcpy(src, "http://www.abc.com:321:123");
	rc = parser_parse_url(src, &url);

	if(rc != ERR_OK || strcmp(url.protocol,"http") != 0  || strcmp(url.host,"www.abc.com") != 0 || url.port != 321)
		++err;

	printf(" => %d errors\n", err);

	return err;
}

int test_sock_q()
{
	int err = 0;
	printf("Testing fifo ring ");

	FifoRing_t fr;
	Error_t rc;
	int* p;
	int data[] = {1, 2, 3, 4};

	printf(".");

	fifo_ring_init(&fr, 3);

	rc = fifo_ring_add (&fr, &data[0]);
	if(rc != ERR_OK)
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 1))
			++err;

	printf(".");

	fifo_ring_init(&fr, 3);

	rc = fifo_ring_add (&fr, &data[0]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[1]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[2]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[3]);
	if(rc != ERR_OUT_OF_BOUNDS)
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 1))
			++err;
	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 2))
			++err;
	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 3))
			++err;
	p = (int*)fifo_ring_get (&fr);
	if(p != NULL)
			++err;

	printf(".");

	fifo_ring_init(&fr, 3);

	rc = fifo_ring_add (&fr, &data[0]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[1]);
	if(rc != ERR_OK)
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 1))
			++err;

	rc = fifo_ring_add (&fr, &data[2]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[3]);
	if(rc != ERR_OK)
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 2))
			++err;
	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 3))
			++err;

	rc = fifo_ring_add (&fr, &data[0]);
	if(rc != ERR_OK)
			++err;
	rc = fifo_ring_add (&fr, &data[1]);
	if(rc != ERR_OK)
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 4))
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 1))
			++err;

	p = (int*)fifo_ring_get (&fr);
	if(p == NULL || (*p != 2))
			++err;

	printf(" => %d errors\n", err);

	return err;
}

int test_hash()
{
	int err = 0;
	printf("Testing hash ");

	Hash_t h;
	Error_t rc;
	long i;
	void* val;
	uint8_t sum = 0, check[10];

	printf(".");

	rc = utils_hash_init(&h, 10);
	if(rc != ERR_OK)
			++err;

	for(i=1; i<10; i++)
	{
		rc = utils_hash_add(&h, (void*)i);

		if(rc != ERR_OK)
			++err;

		check[i] = 1;
	}

	utils_hash_del(&h, (void*)2);
	check[2] = 0;
	utils_hash_del(&h, (void*)3);
	check[3] = 0;
	utils_hash_del(&h, (void*)5);
	check[5] = 0;
	utils_hash_del(&h, (void*)6);
	check[6] = 0;
	utils_hash_del(&h, (void*)7);
	check[7] = 0;

	val = utils_hash_enum(&h, 1);
	check[(long)val] = 0;
	val = utils_hash_enum(&h, 0);
	check[(long)val] = 0;
	val = utils_hash_enum(&h, 0);
	check[(long)val] = 0;
	val = utils_hash_enum(&h, 0);
	check[(long)val] = 0;

	val = utils_hash_enum(&h, 0);
	if(val != NULL)
		++err;

	for(i=1; i<10; i++)
		sum += check[i];

	if(sum !=0)
		++err;

	printf(" => %d errors\n", err);

	return err;
}

void utest_test_all()
{
	int err = 0;

	err += test_time_utils();
	err += test_eggbasket();
	err += test_parse_affinity();
	err += test_parse_url();
	err += test_sock_q();
	err += test_hash();

	if (err == 0)
		printf ("Unit test PASSED\n");
	else
		printf ("Unit test FAILED\n");
}

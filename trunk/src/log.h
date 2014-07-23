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

#ifndef _LOG_H_
#define _LOG_H_

#include "utils.h"

#define LOG(LEVEL, PARAMS, ...) printf(PARAMS, __VA_ARGS__);

#ifdef _DEBUG
	#define LOG_ERR(...) { log_lock();  fprintf(stderr, "%s Error at %s() %s:%d ", utils_get_current_time(), __func__, __FILE__, __LINE__);  fprintf(stderr, __VA_ARGS__);  log_unlock(); }
	#define LOG_DEBUG(...) { log_lock();  fprintf(stdout, "%s Debug at %s() %s:%d ", utils_get_current_time(), __func__, __FILE__, __LINE__);  fprintf(stdout, __VA_ARGS__);  log_unlock(); }
#else
	#define LOG_ERR(...) { log_lock();  fprintf(stderr, "ERROR ");  fprintf(stderr, __VA_ARGS__);  log_unlock(); }
	#define LOG_DEBUG(...)
#endif

/* Thread safe but not super-fast log functions */

void log_init();
void log_lock();
void log_unlock();


#endif /* _LOG_H_ */

/* 
 * Copyright (C) 2011 Jiaju Zhang <jjzhang@suse.de>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include "log.h"
#include "timer.h"

#define MSEC_IN_SEC	1000

extern int poll_timeout;
static LIST_HEAD(timer_head);

unsigned long long current_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec;
}

struct timerlist * add_timer(unsigned long expires,
			     unsigned long data, 
			     void (*function) (unsigned long data))
{
	struct timerlist *timer;

	timer = malloc(sizeof(struct timerlist));
	if (!timer) {
		log_error("failed to alloc mem for timer");
		return NULL;
	}
	memset(timer, 0, sizeof(struct timerlist));

	timer->expires = current_time() + expires;
	timer->data = data;
	timer->function = function;
	list_add_tail(&timer->entry, &timer_head);

	return timer;
}

int del_timer(struct timerlist **timer)
{
	(*timer)->expires = -2;
	(*timer)->data = 0;
	(*timer)->function = NULL;
	*timer = NULL;
	
	return 0;
}

void process_timerlist(void)
{
	struct timerlist *timer, *safe;

	if (list_empty(&timer_head))
		return;

	list_for_each_entry_safe(timer, safe, &timer_head, entry) {
		if (timer->expires == -2) {
			list_del(&timer->entry);
			free(timer);
		} else if (current_time() >= timer->expires) {
			timer->expires = -1;
			timer->function(timer->data);
		}
	}
}

int timerlist_init(void)
{
	poll_timeout = MSEC_IN_SEC;
	return 0;
}

void timerlist_exit(void)
{
	struct timerlist *timer, *safe;

	list_for_each_entry_safe(timer, safe, &timer_head, entry) {
		list_del(&timer->entry);
		free(timer);
	}
}

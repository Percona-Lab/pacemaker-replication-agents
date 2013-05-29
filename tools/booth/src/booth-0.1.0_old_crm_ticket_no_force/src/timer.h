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

#ifndef _TIMER_H
#define _TIMER_H

#include "list.h"

struct timerlist {
	struct list_head entry;
	unsigned long long expires;
	void (*function) (unsigned long);
	unsigned long data;
};

int timerlist_init(void);
struct timerlist * add_timer(unsigned long expires,
			     unsigned long data, 
			     void (*function) (unsigned long data));
int del_timer(struct timerlist **timer);
void timerlist_exit(void);
void process_timerlist(void);
unsigned long long current_time(void);

#endif /* _TIMER_H */

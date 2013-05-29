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

#ifndef _PACEMAKER_H
#define _PACEMAKER_H

struct ticket_handler {
	void (*grant_ticket) (const void *);
	void (*revoke_ticket) (const void *);
	void (*store_ticket) (const void *, int, int, unsigned long long);
	void (*load_ticket) (const void *, int *, int *, unsigned long long *);
};

struct ticket_handler pcmk_handler;

#endif /* _PACEMAKER_H */

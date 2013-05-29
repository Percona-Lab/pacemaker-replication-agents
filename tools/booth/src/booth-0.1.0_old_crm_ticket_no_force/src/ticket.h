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

#ifndef _TICKET_H
#define _TICKET_H

#define DEFAULT_TICKET_EXPIRY	600

int check_ticket(char *ticket);
int check_site(char *site, int *local);
int grant_ticket(char *ticket);
int revoke_ticket(char *ticket);
int list_ticket(char **pdata, unsigned int *len);
int catchup_ticket(char **pdata, unsigned int len);
int ticket_recv(void *msg, int msglen);
int setup_ticket(void);
int get_ticket_info(char *name, int *owner, int *expires);

#endif /* _TICKET_H */

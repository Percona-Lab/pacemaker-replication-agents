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

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>
#include "booth.h"
#include "transport.h"

#define MAX_NODES	16
#define TICKET_ALLOC	16

struct ticket_config {
	int weight[MAX_NODES];
	int expiry;
	char name[BOOTH_NAME_LEN];
};

struct booth_config {
	int node_count;
	int ticket_count;
	transport_layer_t proto;
	uint16_t port;
	struct booth_node node[MAX_NODES];
	struct ticket_config ticket[0];
};

struct booth_config *booth_conf;

int read_config(const char *path);

int check_config(int type);

#endif /* _CONFIG_H */

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

#ifndef _PAXOS_LEASE_H
#define _PAXOS_LEASE_H

#define PLEASE_NAME_LEN		63

#define NOT_CLEAR_RELEASE	0
#define CLEAR_RELEASE		1

typedef long pl_handle_t;

struct paxos_lease_result {
	char name[PLEASE_NAME_LEN+1];
	int owner;
	int ballot;
	unsigned long long expires;
};

struct paxos_lease_operations {
	int (*get_myid) (void);
	int (*send) (unsigned long id, void *value, int len);
	int (*broadcast) (void *value, int len);
	int (*catchup) (const void *name, int *owner, int *ballot,
			unsigned long long *expires);
	int (*notify) (pl_handle_t handle, struct paxos_lease_result *result);
};

pl_handle_t paxos_lease_init(const void *name,
			     unsigned int namelen,
			     int expiry,
			     int number,
			     int failover,
			     unsigned char *role,
			     int *prio,
			     const struct paxos_lease_operations *pl_op);

int paxos_lease_on_receive(void *msg, int msglen);

int paxos_lease_acquire(pl_handle_t handle,
			int clear,
			int renew,
			void (*end_acquire) (pl_handle_t handle, int result));
/*
int paxos_lease_owner_get(const void *name);

int paxos_lease_epoch_get(const void *name);

int paxos_lease_timeout(const void *name);
*/
int paxos_lease_status_recovery(pl_handle_t handle);

int paxos_lease_release(pl_handle_t handle,
			void (*end_release) (pl_handle_t handle, int result));

int paxos_lease_exit(pl_handle_t handle);

#endif /* _PAXOS_LEASE_H */

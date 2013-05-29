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

#ifndef _PAXOS_H
#define _PAXOS_H

#define PAXOS_NAME_LEN	63

#define PROPOSER	0x4
#define ACCEPTOR	0x2
#define LEARNER		0x1

typedef long ps_handle_t;
typedef long pi_handle_t;

struct paxos_operations {
	int (*get_myid) (void);
	int (*send) (unsigned long id, void *value, int len);
	int (*broadcast) (void *value, int len);
	int (*catchup) (pi_handle_t handle);
	int (*prepare) (pi_handle_t handle, void *extra);
	int (*promise) (pi_handle_t handle, void *extra);
	int (*is_prepared) (pi_handle_t handle, void *extra);
	int (*propose) (pi_handle_t handle, void *extra,
			int round, void *value);
	int (*accepted) (pi_handle_t handle, void *extra,
			 int round, void *value);
	int (*commit) (pi_handle_t handle, void *extra, int round);
	int (*learned) (pi_handle_t handle, void *extra, int round);
};

int paxos_recvmsg(void *msg, int msglen);

ps_handle_t paxos_space_init(const void *name,
			     unsigned int number,
			     unsigned int extralen,
			     unsigned int valuelen,
			     const unsigned char *role,
			     const struct paxos_operations *p_op);

pi_handle_t paxos_instance_init(ps_handle_t handle,
				const void *name,
				int *prio);

int paxos_round_request(pi_handle_t handle,
			void *value,
			int *round,
			void (*end_request) (pi_handle_t handle,
					     int round,
					     int result));

int paxos_round_discard(pi_handle_t handle, int round);

int paxos_leader_get(pi_handle_t handle, int *round);

int paxos_recovery_status_get(pi_handle_t handle);

int paxos_recovery_status_set(pi_handle_t handle, int recovery);

int paxos_catchup(pi_handle_t handle);

int paxos_propose(pi_handle_t handle, void *value, int round);

int paxos_instance_exit(pi_handle_t handle);

int paxos_space_exit(ps_handle_t handle);

#endif /* _PAXOS_H */

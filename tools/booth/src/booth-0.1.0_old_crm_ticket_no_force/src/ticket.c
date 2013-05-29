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
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "ticket.h"
#include "config.h"
#include "pacemaker.h"
#include "list.h"
#include "log.h"
#include "booth.h"
#include "timer.h"
#include "paxos_lease.h"
#include "paxos.h"

#define PAXOS_MAGIC		0xDB12
#define TK_LINE			256

#define CATCHED_VALID_TMSG	1

struct booth_msghdr {
        uint16_t magic;
        uint16_t checksum;
        uint32_t len;
} __attribute__((packed));

struct ticket_msg {
	char id[BOOTH_NAME_LEN+1];
	uint32_t owner;
	uint32_t expiry;
	uint32_t ballot;
	uint32_t result;
} __attribute__((packed));

struct ticket {
	char id[BOOTH_NAME_LEN+1];
	pl_handle_t handle;
	int owner;
	int expiry;
	int ballot;
	unsigned long long expires;
	struct list_head list;
};

static LIST_HEAD(ticket_list);

static unsigned char *role;

int check_ticket(char *ticket)
{
	int i;

	if (!booth_conf)
		return 0;

	for (i = 0; i < booth_conf->ticket_count; i++) {
		if (!strcmp(booth_conf->ticket[i].name, ticket))
			return 1;
	}

	return 0;
}

int check_site(char *site, int *local)
{
	int i;

	if (!booth_conf)
		return 0;

	for (i = 0; i < booth_conf->node_count; i++) {
		if (booth_conf->node[i].type == SITE
		    && !strcmp(booth_conf->node[i].addr, site)) {
			*local = booth_conf->node[i].local;
			return 1;
		}
	}

	return 0;
}

static int * ticket_priority(int i)
{
	int j;

	/* TODO: need more precise check */
	for (j = 0; j < booth_conf->node_count; j++) {
		if (booth_conf->ticket[i].weight[j] == 0)
			return NULL;
	}
	return booth_conf->ticket[i].weight;
}

static int ticket_get_myid(void)
{
	return booth_transport[booth_conf->proto].get_myid();
}

static void end_acquire(pl_handle_t handle, int error)
{
	struct ticket *tk;
	int found = 0;

	log_debug("enter end_acquire");
	list_for_each_entry(tk, &ticket_list, list) {
		if (tk->handle == handle) {
			found = 1;
			break;
		}
	}

	if (!found) {
		log_error("BUG: ticket handle %ld does not exist", handle);
		return;
	}

	if (error)
		log_info("ticket %s was granted failed (site %d), error:%s",
				tk->id, ticket_get_myid(), strerror(error));
	else
		log_info("ticket %s was granted successfully (site %d)",
				tk->id, ticket_get_myid());
	log_debug("exit end_acquire");
}

static void end_release(pl_handle_t handle, int error)
{
	struct ticket *tk;
	int found = 0;

	log_debug("enter end_release");
	list_for_each_entry(tk, &ticket_list, list) {
		if (tk->handle == handle) {
			found = 1;
			break;
		}
	}

	if (!found) {
		log_error("BUG: ticket handle %ld does not exist", handle);
		return;
	}

	if (error)
		log_info("ticket %s was reovked failed (site %d), error:%s",
				tk->id, ticket_get_myid(), strerror(error));
	else
		log_info("ticket %s was reovked successfully (site %d)",
				tk->id, ticket_get_myid());

	log_debug("exit end_release");
}

static int ticket_send(unsigned long id, void *value, int len)
{
	int i, rv = -1;
	struct booth_node *to = NULL;
	struct booth_msghdr *hdr;
	void *buf;

	for (i = 0; i < booth_conf->node_count; i++) {
		if (booth_conf->node[i].nodeid == id)
			to = &booth_conf->node[i];
	}
	if (!to)
		return rv;

	buf = malloc(sizeof(struct booth_msghdr) + len);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, sizeof(struct booth_msghdr) + len);
	hdr = buf;
	hdr->magic = htons(PAXOS_MAGIC);
	hdr->len = htonl(sizeof(struct booth_msghdr) + len);
	memcpy((char *)buf + sizeof(struct booth_msghdr), value, len);

	rv = booth_transport[booth_conf->proto].send(
		(unsigned long)to, buf, sizeof(struct booth_msghdr) + len);

	free(buf);
	return rv;
}

static int ticket_broadcast(void *value, int len)
{
	void *buf;
	struct booth_msghdr *hdr;
	int rv;

	buf = malloc(sizeof(struct booth_msghdr) + len);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, sizeof(struct booth_msghdr) + len);
	hdr = buf;
	hdr->magic = htons(PAXOS_MAGIC);
	hdr->len = htonl(sizeof(struct booth_msghdr) + len);
	memcpy((char *)buf + sizeof(struct booth_msghdr), value, len);

	rv = booth_transport[booth_conf->proto].broadcast(
			buf, sizeof(struct booth_msghdr) + len);

	free(buf);
	return rv;	
}
#if 0
static int ticket_read(const void *name, int *owner, int *ballot, 
		       unsigned long long *expires)
{
	struct ticket *tk;
	int found = 0;
	
	list_for_each_entry(tk, &ticket_list, list) {
		if (!strcmp(tk->id, name)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		log_error("BUG: ticket_read failed (ticket %s does not exist)",
			  (char *)name);
		return -1;
	}

	pcmk_handler.load_ticket(tk->id, &tk->owner, &tk->ballot, &tk->expires);
	*owner = tk->owner;
	*expires = tk->expires;
	*ballot = tk->ballot;
 
	return 0;
}
#endif
static int ticket_parse(struct ticket_msg *tmsg)
{
	struct ticket *tk;
	int found = 0;

	list_for_each_entry(tk, &ticket_list, list) {
		if (!strcmp(tk->id, tmsg->id)) {
			found = 1;
			if (tk->ballot < tmsg->ballot)
				tk->ballot = tmsg->ballot;
			if (CATCHED_VALID_TMSG == tmsg->result) {
				tk->owner = tmsg->owner;
				tk->expires = current_time() + tmsg->expiry;
			}
			break;
		}
	}

	if (!found)
		return -1;
	else
		return 0;
}

static int ticket_catchup(const void *name, int *owner, int *ballot,
			  unsigned long long *expires)
{	
	struct ticket *tk;
	int i, s, buflen, rv = 0;
	char *buf = NULL;
	struct boothc_header *h;
	struct ticket_msg *tmsg;
	int myid = ticket_get_myid();

	if (booth_conf->node[myid].type != ARBITRATOR) {
		list_for_each_entry(tk, &ticket_list, list) {
			if (!strcmp(tk->id, name)) {
				pcmk_handler.load_ticket(tk->id,
							 &tk->owner,
							 &tk->ballot,
							 &tk->expires);
				if (current_time() >= tk->expires) {
					tk->owner = -1;
					tk->expires = 0;
				} 
			}
		}
	}

	buflen = sizeof(struct boothc_header) + sizeof(struct ticket_msg);
	buf = malloc(buflen);
	if (!buf)
		return -ENOMEM;
	memset(buf, 0, buflen);

	h = (struct boothc_header *)buf;
	h->magic = BOOTHC_MAGIC;
	h->version = BOOTHC_VERSION;
	h->cmd = BOOTHC_CMD_CATCHUP;
	h->len = sizeof(struct ticket_msg);
	tmsg = (struct ticket_msg *)(buf + sizeof(struct boothc_header));
	
	for (i = 0; i < booth_conf->node_count; i++) {
		if (booth_conf->node[i].type == SITE &&
		    !(booth_conf->node[i].local)) {
			strncpy(tmsg->id, name, BOOTH_NAME_LEN + 1);
			log_debug("attempting catchup from %s", booth_conf->node[i].addr);
			s = booth_transport[TCP].open(&booth_conf->node[i]);
			if (s < 0)
				continue;
			log_debug("connected to %s", booth_conf->node[i].addr);
			rv = booth_transport[TCP].send(s, buf, buflen);
			if (rv < 0) {
				booth_transport[TCP].close(s);
				continue;
			}
			log_debug("sent catchup command to %s", booth_conf->node[i].addr);
			memset(tmsg, 0, sizeof(struct ticket_msg));
			rv = booth_transport[TCP].recv(s, buf, buflen);
			if (rv < 0) {
				booth_transport[TCP].close(s);
				continue;
			}
			booth_transport[TCP].close(s);
			ticket_parse(tmsg);
			memset(tmsg, 0, sizeof(struct ticket_msg)); 
		}
	}
		
	list_for_each_entry(tk, &ticket_list, list) {
		if (!strcmp(tk->id, name)) {
			if (booth_conf->node[myid].type != ARBITRATOR) {
				if (current_time() >= tk->expires) {
					tk->owner = -1;
					tk->expires = 0;
				}
				pcmk_handler.store_ticket(tk->id,
							  tk->owner,
							  tk->ballot,
							  tk->expires);
				if (tk->owner == myid)
					pcmk_handler.grant_ticket(tk->id);
				else
					pcmk_handler.revoke_ticket(tk->id);
			}
			*owner = tk->owner;
			*expires = tk->expires;
			*ballot = tk->ballot;
		}
	}

	free(buf);
	return rv;
}

static int ticket_write(pl_handle_t handle, struct paxos_lease_result *result)
{
	struct ticket *tk;
	int found = 0;
	
	list_for_each_entry(tk, &ticket_list, list) {
		if (tk->handle == handle) {
			found = 1;
			break;
		}
	}
	if (!found) {
		log_error("BUG: ticket_write failed "
			  "(ticket handle %ld does not exist)", handle);
		return -1;
	}

	tk->owner = result->owner;
	tk->expires = result->expires;
	tk->ballot = result->ballot;

	if (tk->owner == ticket_get_myid()) {
		pcmk_handler.store_ticket(tk->id, tk->owner, tk->ballot, tk->expires);
		pcmk_handler.grant_ticket(tk->id);
	} else if (tk->owner == -1) {
		pcmk_handler.store_ticket(tk->id, tk->owner, tk->ballot, tk->expires);
		pcmk_handler.revoke_ticket(tk->id);
	} else
		pcmk_handler.store_ticket(tk->id, tk->owner, tk->ballot, tk->expires);

	return 0; 
}

static void ticket_status_recovery(pl_handle_t handle)
{
	paxos_lease_status_recovery(handle);
}

int ticket_recv(void *msg, int msglen)
{
	struct booth_msghdr *hdr;
	char *data;

	hdr = msg;
	if (ntohs(hdr->magic) != PAXOS_MAGIC ||
	    ntohl(hdr->len) != msglen) {
		log_error("message received error");
		return -1;
	}
	data = (char *)msg + sizeof(struct booth_msghdr);

	return paxos_lease_on_receive(data,
				      msglen - sizeof(struct booth_msghdr));
}

int grant_ticket(char *ticket)
{
	struct ticket *tk;
	int found = 0;

	list_for_each_entry(tk, &ticket_list, list) {
		if (!strcmp(tk->id, ticket)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		log_error("ticket %s does not exist", ticket);
		return BOOTHC_RLT_SYNC_FAIL;
	}

	if (tk->owner == ticket_get_myid())
		return BOOTHC_RLT_SYNC_SUCC;
	else {
		int ret = paxos_lease_acquire(tk->handle, CLEAR_RELEASE,
				1, end_acquire);
		if (ret >= 0)
			tk->ballot = ret;
		return (ret < 0)? BOOTHC_RLT_SYNC_FAIL: BOOTHC_RLT_ASYNC;
	}
}

int revoke_ticket(char *ticket)
{
	struct ticket *tk;
	int found = 0;

	list_for_each_entry(tk, &ticket_list, list) {
		if (!strcmp(tk->id, ticket)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		log_error("ticket %s does not exist", ticket);
		return BOOTHC_RLT_SYNC_FAIL;
	}

	if (tk->owner == -1)
		return BOOTHC_RLT_SYNC_SUCC;
	else {
		int ret = paxos_lease_release(tk->handle, end_release);
		if (ret >= 0)
			tk->ballot = ret;
		return (ret < 0)? BOOTHC_RLT_SYNC_FAIL: BOOTHC_RLT_ASYNC;
	}	
}

int get_ticket_info(char *name, int *owner, int *expires)
{
	struct ticket *tk;

	list_for_each_entry(tk, &ticket_list, list) {
		if (!strncmp(tk->id, name, BOOTH_NAME_LEN + 1)) {
			if(owner)
				*owner = tk->owner;
			if(expires)
				*expires = tk->expires;
			return 0;
		}
	}

	return -1;
}

int list_ticket(char **pdata, unsigned int *len)
{
	struct ticket *tk;
	char timeout_str[100];
	char node_name[BOOTH_NAME_LEN];
	char tmp[TK_LINE];

	*pdata = NULL;
	*len = 0;
	list_for_each_entry(tk, &ticket_list, list) {
		memset(tmp, 0, TK_LINE);
		strncpy(timeout_str, "INF", sizeof(timeout_str));
		strncpy(node_name, "None", sizeof(node_name));

		if (tk->owner < MAX_NODES && tk->owner > -1)
			strncpy(node_name, booth_conf->node[tk->owner].addr,
					sizeof(node_name));
		if (tk->expires != 0)
			strftime(timeout_str, sizeof(timeout_str), "%Y/%m/%d %H:%M:%S",
					localtime((time_t *)&tk->expires));
		snprintf(tmp, TK_LINE, "ticket: %s, owner: %s, expires: %s\n",
			 tk->id, node_name, timeout_str);
		*pdata = realloc(*pdata, *len + TK_LINE);
		if (*pdata == NULL)
			return -ENOMEM;
		memset(*pdata + *len, 0, TK_LINE);
		memcpy(*pdata + *len, tmp, TK_LINE);
		*len += TK_LINE;
	}

	return 0;
}

int catchup_ticket(char **pdata, unsigned int len)
{
	struct ticket_msg *tmsg;
	struct ticket *tk;

	assert(len == sizeof(struct ticket_msg));
	tmsg = (struct ticket_msg *)(*pdata);
	list_for_each_entry(tk, &ticket_list, list) {
		if (strcmp(tk->id, tmsg->id))
			continue;

		tmsg->ballot = tk->ballot;
		if (tk->owner == ticket_get_myid()
				&& current_time() < tk->expires) {
			tmsg->result = CATCHED_VALID_TMSG;
			tmsg->expiry = tk->expires - current_time();
			tmsg->owner = tk->owner;
		}
	}

	return 0;
}

const struct paxos_lease_operations ticket_operations = {
	.get_myid	= ticket_get_myid,
	.send		= ticket_send,
	.broadcast	= ticket_broadcast,
	.catchup	= ticket_catchup,
	.notify		= ticket_write,
};

int setup_ticket(void)
{
	struct ticket *tk, *tmp;
	int i, rv;
	pl_handle_t plh;
	int myid;
		
	role = malloc(booth_conf->node_count * sizeof(unsigned char));
	if (!role)
		return -ENOMEM;
	memset(role, 0, booth_conf->node_count * sizeof(unsigned char));
	for (i = 0; i < booth_conf->node_count; i++) {
		if (booth_conf->node[i].type == SITE)
			role[i] = PROPOSER | ACCEPTOR | LEARNER;
		else if (booth_conf->node[i].type == ARBITRATOR)
			role[i] = ACCEPTOR | LEARNER;
	}

	for (i = 0; i < booth_conf->ticket_count; i++) {
		tk = malloc(sizeof(struct ticket));
		if (!tk) {
			rv = -ENOMEM;
			goto out;
		}
		memset(tk, 0, sizeof(struct ticket));
		strcpy(tk->id, booth_conf->ticket[i].name);
		tk->owner = -1;
		tk->expiry = booth_conf->ticket[i].expiry;
		list_add_tail(&tk->list, &ticket_list); 

		plh = paxos_lease_init(tk->id,
				       BOOTH_NAME_LEN,
				       tk->expiry,
				       booth_conf->node_count,
				       1,
				       role,
				       ticket_priority(i),
				       &ticket_operations);
		if (plh <= 0) {
			log_error("paxos lease initialization failed");
			rv = plh;
			goto out;
		}
		tk->handle = plh;
	}

	myid = ticket_get_myid();
	assert(myid < booth_conf->node_count);
	if (role[myid] & ACCEPTOR) {
		list_for_each_entry(tk, &ticket_list, list) {
			ticket_status_recovery(tk->handle);
		}
	}

	return 0;

out:
	list_for_each_entry_safe(tk, tmp, &ticket_list, list) {
		list_del(&tk->list);
	}
	free(role);

	return rv;
}

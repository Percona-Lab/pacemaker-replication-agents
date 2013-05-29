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
#include "list.h"
#include "paxos.h"
#include "log.h"

typedef enum {
	INIT = 1,
	PREPARING,
	PROMISING,
	PROPOSING,
	ACCEPTING,
	RECOVERY,
	COMMITTED,
} paxos_state_t;

struct proposal {
	int ballot_number;
	char value[0];
};

struct learned {
	int ballot;
	int number;
};

struct paxos_msghdr {
	paxos_state_t state;
	int from;
	char psname[PAXOS_NAME_LEN+1];
	char piname[PAXOS_NAME_LEN+1];
	int ballot_number;
	int proposer_id;
	unsigned int extralen;
	unsigned int valuelen;
};

struct proposer {
	int state;
	int ballot;
	int open_number;
	int accepted_number;
	int proposed;
	struct proposal *proposal;
};

struct acceptor {
	int state;
	int highest_promised;
    int number;
	struct proposal *accepted_proposal;
};

struct learner {
	int state;
	int learned_max;
	int learned_ballot;
	struct learned learned[0];
};

struct paxos_space;
struct paxos_instance;

struct proposer_operations {
	void (*prepare) (struct paxos_instance *,
			 int *);
	void (*propose) (struct paxos_space *,
			 struct paxos_instance *,
			 void *, int);
	void (*commit) (struct paxos_space *,
			struct paxos_instance *,
			void *, int);
};

struct acceptor_operations {
	void (*promise) (struct paxos_space *,
			 struct paxos_instance *,
			 void *, int);
	void (*accepted) (struct paxos_space *,
			  struct paxos_instance *,
			  void *, int);
};

struct learner_operations {
	void (*response) (struct paxos_space *,
			  struct paxos_instance *,
			  void *, int);
};
	

struct paxos_space {
	char name[PAXOS_NAME_LEN+1];
	unsigned int number;
	unsigned int extralen;
	unsigned int valuelen;
	const unsigned char *role;
	const struct paxos_operations *p_op;
	const struct proposer_operations *r_op;
	const struct acceptor_operations *a_op;
	const struct learner_operations *l_op;
	struct list_head list;
	struct list_head pi_head;
};

struct paxos_instance {
	char name[PAXOS_NAME_LEN+1];
	int round;
	int *prio;
	struct proposer *proposer;
	struct acceptor *acceptor;
	struct learner *learner;
	void (*end) (pi_handle_t pih, int round, int result);
	struct list_head list;
	struct paxos_space *ps;
};

static LIST_HEAD(ps_head);

static int have_quorum(struct paxos_space *ps, int member)
{
	int i, sum = 0;
    
	for (i = 0; i < ps->number; i++) {
		if (ps->role[i] & ACCEPTOR)
			sum++;
	}

	if (member * 2 > sum)
		return 1;
	else
		return 0;
}

static int next_ballot_number(struct paxos_instance *pi)
{
	int ballot;
	int myid = pi->ps->p_op->get_myid();

	if (pi->prio)
		ballot = pi->prio[myid];
	else
		ballot = myid;

	while (ballot <= pi->round)
		ballot += pi->ps->number;

	return ballot;
}

static void proposer_prepare(struct paxos_instance *pi, int *round)
{
	struct paxos_msghdr *hdr;
	void *msg, *extra;
	int msglen = sizeof(struct paxos_msghdr) + pi->ps->extralen;
	int ballot;

	log_debug("preposer prepare ...");
	msg = malloc(msglen);
	if (!msg) {
		log_error("no mem for msg");
		*round = -ENOMEM;
		return;
	}
	memset(msg, 0, msglen);
	hdr = msg;
	extra = (char *)msg + sizeof(struct paxos_msghdr);

	if (*round > pi->round)
		pi->round = *round;
	ballot = next_ballot_number(pi);
	pi->proposer->ballot = ballot;
    
    log_debug("new ballot number = %d",ballot);

	hdr->state = htonl(PREPARING);
	hdr->from = htonl(pi->ps->p_op->get_myid());
	hdr->proposer_id = hdr->from;
	strcpy(hdr->psname, pi->ps->name);
	strcpy(hdr->piname, pi->name);
	hdr->ballot_number = htonl(ballot);
	hdr->extralen = htonl(pi->ps->extralen);

	if (pi->ps->p_op->prepare &&
		pi->ps->p_op->prepare((pi_handle_t)pi, extra) < 0)
		return;

	if (pi->ps->p_op->broadcast)
		pi->ps->p_op->broadcast(msg, msglen);
	else {
		int i;
		for (i = 0; i < pi->ps->number; i++) {
			if (pi->ps->role[i] & ACCEPTOR)
				pi->ps->p_op->send(i, msg, msglen);
		}
	}

	free(msg);
	*round = ballot;
}

static void proposer_propose(struct paxos_space *ps,
			     struct paxos_instance *pi,
			     void *msg, int msglen)
{
	struct paxos_msghdr *hdr;
	pi_handle_t pih = (pi_handle_t)pi;
	void *extra, *value, *message;
	int ballot;

	log_debug("proposer propose ...");
	if (msglen != sizeof(struct paxos_msghdr) + ps->extralen) {
		log_error("message length incorrect, "
			  "msglen: %d, msghdr len: %lu, extralen: %u",
			  msglen, (long)sizeof(struct paxos_msghdr),
			  ps->extralen);
		return;
	}
	hdr = msg;

	ballot = ntohl(hdr->ballot_number);
	if (pi->proposer->ballot != ballot) {
		log_debug("not the same ballot, proposer ballot: %d, "
			  "received ballot: %d", pi->proposer->ballot, ballot);
		return;
	}

	extra = (char *)msg + sizeof(struct paxos_msghdr);
	if (ps->p_op->is_prepared) {
		if (ps->p_op->is_prepared(pih, extra))
			pi->proposer->open_number++;
	} else
		pi->proposer->open_number++;

	if (!have_quorum(ps, pi->proposer->open_number)) {
		return;
    }

	if (pi->proposer->proposed) {
		return;
    }
	pi->proposer->proposed = 1;

	value = pi->proposer->proposal->value;
	if (ps->p_op->propose
		&& ps->p_op->propose(pih, extra, ballot, value) < 0) {
		return;
    }

	hdr->valuelen = htonl(ps->valuelen); 
	message = malloc(msglen + ps->valuelen);
	if (!message) {
		log_error("no mem for value");
		return;
	}
	memset(message, 0, msglen + ps->valuelen);
	memcpy(message, msg, msglen);
	memcpy((char *)message + msglen, value, ps->valuelen);
	pi->proposer->state = PROPOSING;
	hdr = message;
	hdr->from = htonl(ps->p_op->get_myid());
	hdr->state = htonl(PROPOSING);

	if (ps->p_op->broadcast)
		ps->p_op->broadcast(message, msglen + ps->valuelen);
	else {
		int i;
		for (i = 0; i < ps->number; i++) {
			if (ps->role[i] & ACCEPTOR)
				ps->p_op->send(i, message,
					       msglen + ps->valuelen);
		}
	}
	free(message);
}

static void proposer_commit(struct paxos_space *ps,
			    struct paxos_instance *pi,
			    void *msg, int msglen)
{
	struct paxos_msghdr *hdr;
	pi_handle_t pih = (pi_handle_t)pi;
	void *extra;
	int ballot;

	log_debug("proposer commit ...");
	if (msglen != sizeof(struct paxos_msghdr) + ps->extralen) {
		log_error("message length incorrect, "
			  "msglen: %d, msghdr len: %lu, extralen: %u",
			  msglen, (long)sizeof(struct paxos_msghdr),
			  ps->extralen);
		return;
	}
	
	extra = (char *)msg + sizeof(struct paxos_msghdr);
	hdr = msg;

        ballot = ntohl(hdr->ballot_number);
        if (pi->proposer->ballot != ballot) {
		log_debug("not the same ballot, proposer ballot: %d, "
			  "received ballot: %d", pi->proposer->ballot, ballot);
                return;
	}

	pi->proposer->accepted_number++;

	if (!have_quorum(ps, pi->proposer->accepted_number))
		return;

	if (pi->proposer->state == COMMITTED)
		return;

	pi->round = ballot;
	if (ps->p_op->commit
		&& ps->p_op->commit(pih, extra, pi->round) < 0)
		return;
	pi->proposer->state = COMMITTED;

	if (pi->end)
		pi->end(pih, pi->round, 0);	
}

static void acceptor_promise(struct paxos_space *ps,
			     struct paxos_instance *pi,
			     void *msg, int msglen)
{
	struct paxos_msghdr *hdr;
	unsigned long to;
	pi_handle_t pih = (pi_handle_t)pi;
	void *extra;

	log_debug("acceptor promise ...");
	if (pi->acceptor->state == RECOVERY) {
		log_debug("still in recovery");
		return;
	}

	if (msglen != sizeof(struct paxos_msghdr) + ps->extralen) {
		log_error("message length incorrect, "
			  "msglen: %d, msghdr len: %lu, extralen: %u",
			  msglen, (long)sizeof(struct paxos_msghdr),
			  ps->extralen);
		return;
	}
	hdr = msg;
	extra = (char *)msg + sizeof(struct paxos_msghdr);

    log_debug("ballot number: %d, highest promised: %d",
              ntohl(hdr->ballot_number),
              pi->acceptor->highest_promised);
	if (ntohl(hdr->ballot_number) < pi->acceptor->highest_promised) {
		log_debug("ballot number: %d, highest promised: %d",
			  ntohl(hdr->ballot_number),
			  pi->acceptor->highest_promised);
		return;
	}

	if (ps->p_op->promise
		&& ps->p_op->promise(pih, extra) < 0)
		return;

	pi->acceptor->highest_promised = ntohl(hdr->ballot_number);
	pi->acceptor->state = PROMISING;
	to = ntohl(hdr->from);
	hdr->from = htonl(ps->p_op->get_myid());
	hdr->state = htonl(PROMISING);
	ps->p_op->send(to, msg, msglen);	
}

static void acceptor_accepted(struct paxos_space *ps,
			      struct paxos_instance *pi,
			      void *msg, int msglen)
{
	struct paxos_msghdr *hdr;
	unsigned long to;
	pi_handle_t pih = (pi_handle_t)pi;
	void *extra, *value;
	int myid = ps->p_op->get_myid();
	int ballot;

	log_debug("acceptor accepted ...");
	if (pi->acceptor->state == RECOVERY) {
		log_debug("still in recovery");
		return;
	}

	if (msglen != sizeof(struct paxos_msghdr) + ps->extralen + ps->valuelen) {
		log_error("message length incorrect, msglen: "
			  "%d, msghdr len: %lu, extralen: %u, valuelen: %u",
			  msglen, (long)sizeof(struct paxos_msghdr), ps->extralen,
			  ps->valuelen);
		return;
	}
	hdr = msg;
	extra = (char *)msg + sizeof(struct paxos_msghdr);
	ballot = ntohl(hdr->ballot_number);

	if (ballot < pi->acceptor->highest_promised) {
		log_debug("ballot: %d, highest promised: %d",
			  ballot, pi->acceptor->highest_promised);
		return;
	}

	value = pi->acceptor->accepted_proposal->value;
	memcpy(value, (char *)msg + sizeof(struct paxos_msghdr) + ps->extralen,
	       ps->valuelen);

	if (ps->p_op->accepted
		&& ps->p_op->accepted(pih, extra, ballot, value) < 0)
		return;

	pi->acceptor->state = ACCEPTING;
	to = ntohl(hdr->from);
	hdr->from = htonl(myid);
	hdr->state = htonl(ACCEPTING);

	if (ps->p_op->broadcast)
		ps->p_op->broadcast(msg, sizeof(struct paxos_msghdr)
						+ ps->extralen);
	else {
		int i;
		for (i = 0; i < ps->number; i++) {
			if (ps->role[i] & LEARNER)
				ps->p_op->send(i, msg,
					       sizeof(struct paxos_msghdr)
					       + ps->extralen);
		}
		if (!(ps->role[to] & LEARNER))
			ps->p_op->send(to, msg, sizeof(struct paxos_msghdr)
						+ ps->extralen);	
	}
}

static void learner_response(struct paxos_space *ps,
			     struct paxos_instance *pi,
			     void *msg, int msglen)
{
	struct paxos_msghdr *hdr;
	pi_handle_t pih = (pi_handle_t)pi;
	void *extra;
	int i, unused = 0, found = 0;
	int ballot;

	log_debug("learner response ...");
	if (msglen != sizeof(struct paxos_msghdr) + ps->extralen) {
		log_error("message length incorrect, "
			  "msglen: %d, msghdr len: %lu, extralen: %u",
			  msglen, (long)sizeof(struct paxos_msghdr),
			  ps->extralen);
		return;
	}
	hdr = msg;	
	extra = (char *)msg + sizeof(struct paxos_msghdr);
	ballot = ntohl(hdr->ballot_number);

	for (i = 0; i < ps->number; i++) {
		if (!pi->learner->learned[i].ballot) {
			unused = i;
			break;
		}
		if (pi->learner->learned[i].ballot == ballot) {
			pi->learner->learned[i].number++;
			if (pi->learner->learned[i].number
			    > pi->learner->learned_max)
				pi->learner->learned_max
					= pi->learner->learned[i].number;
			found = 1;
			break;
		}
	}
	if (!found) {
		pi->learner->learned[unused].ballot = ntohl(hdr->ballot_number);
		pi->learner->learned[unused].number = 1;
	}

	if (!have_quorum(ps, pi->learner->learned_max))
		return;

	if (ps->p_op->learned)
		ps->p_op->learned(pih, extra, ballot);
}

const struct proposer_operations generic_proposer_operations = {
	.prepare		= proposer_prepare,
	.propose		= proposer_propose,
	.commit			= proposer_commit,
};

const struct acceptor_operations generic_acceptor_operations = {
	.promise		= acceptor_promise,
	.accepted		= acceptor_accepted,
};

const struct learner_operations generic_learner_operations = {
	.response		= learner_response,
}; 

ps_handle_t paxos_space_init(const void *name,
			     unsigned int number,
			     unsigned int extralen,
			     unsigned int valuelen,
			     const unsigned char *role,
			     const struct paxos_operations *p_op)
{
	struct paxos_space *ps;

	list_for_each_entry(ps, &ps_head, list) {
		if (!strcmp(ps->name, name)) {
			log_info("paxos space (%s) has already been "
				  "initialized", (char *)name);
			return -EEXIST;
		}
	}
	
	if (!number || !valuelen || !p_op || !p_op->get_myid || !p_op->send) {
		log_error("invalid agruments");
		return -EINVAL;
	}

	ps = malloc(sizeof(struct paxos_space));
	if (!ps) {
		log_error("no mem for paxos space");
		return -ENOMEM;
	}
	memset(ps, 0, sizeof(struct paxos_space));

	strncpy(ps->name, name, PAXOS_NAME_LEN + 1);
	ps->number = number;
	ps->extralen = extralen;
	ps->valuelen = valuelen;
	ps->role = role;
	ps->p_op = p_op;
	ps->r_op = &generic_proposer_operations;
	ps->a_op = &generic_acceptor_operations;
	ps->l_op = &generic_learner_operations;

	list_add_tail(&ps->list, &ps_head);
	INIT_LIST_HEAD(&ps->pi_head);

	return (ps_handle_t)ps;
}

pi_handle_t paxos_instance_init(ps_handle_t handle, const void *name, int *prio)
{
	struct paxos_space *ps = (struct paxos_space *)handle;
	struct paxos_instance *pi;
	struct proposer *proposer = NULL;
	struct acceptor *acceptor = NULL;
	struct learner *learner = NULL;
	int myid, valuelen, rv;

	list_for_each_entry(pi, &ps->pi_head, list) {
		if (!strcmp(pi->name, name))
			return (pi_handle_t)pi;
	}

	if (handle <= 0 || !ps->p_op || !ps->p_op->get_myid) {
		log_error("invalid agruments");
		rv = -EINVAL;
		goto out;
	}
	myid = ps->p_op->get_myid();
	valuelen = ps->valuelen; 

	pi = malloc(sizeof(struct paxos_instance));
	if (!pi) {
		log_error("no mem for paxos instance");
		rv = -ENOMEM;
		goto out;
	}
	memset(pi, 0, sizeof(struct paxos_instance));
	strncpy(pi->name, name, PAXOS_NAME_LEN + 1);

	if (prio) {
		pi->prio = malloc(ps->number * sizeof(int));
		if (!pi->prio) {
			log_error("no mem for prio");
			rv = -ENOMEM;
			goto out_pi;
		}
		memcpy(pi->prio, prio, ps->number * sizeof(int));
	}

	if (ps->role[myid] & PROPOSER) {
		proposer = malloc(sizeof(struct proposer));
		if (!proposer) {
			log_error("no mem for proposer");
			rv = -ENOMEM;
			goto out_prio;
		}
		memset(proposer, 0, sizeof(struct proposer));
		proposer->state = INIT;

		proposer->proposal = malloc(sizeof(struct proposal) + valuelen);
		if (!proposer->proposal) {
			log_error("no mem for proposal");
			rv = -ENOMEM;
			goto out_proposer;
		}
		memset(proposer->proposal, 0,
		       sizeof(struct proposal) + valuelen);
		pi->proposer = proposer;
	}

	if (ps->role[myid] & ACCEPTOR) {
		acceptor = malloc(sizeof(struct acceptor));
		if (!acceptor) {
			log_error("no mem for acceptor");
			rv = -ENOMEM;
			goto out_proposal;
		}
		memset(acceptor, 0, sizeof(struct acceptor));
		acceptor->state = INIT;

		acceptor->accepted_proposal = malloc(sizeof(struct proposal)
						     + valuelen);
		if (!acceptor->accepted_proposal) {
			log_error("no mem for accepted proposal");
			rv = -ENOMEM;
			goto out_acceptor;
		}
		memset(acceptor->accepted_proposal, 0,
		       sizeof(struct proposal) + valuelen);
		pi->acceptor = acceptor;
	
		if (ps->p_op->catchup)
			pi->acceptor->state = RECOVERY;
		else
			pi->acceptor->state = INIT;
	}

	if (ps->role[myid] & LEARNER) {
		learner = malloc(sizeof(struct learner)
				 + ps->number * sizeof(struct learned));
		if (!learner) {
			log_error("no mem for learner");
			rv = -ENOMEM;
			goto out_accepted_proposal;
		}
		memset(learner, 0,
		       sizeof(struct learner) 
		       + ps->number * sizeof(struct learned));
		learner->state = INIT;
		pi->learner = learner;
	}

	pi->ps = ps;
	list_add_tail(&pi->list, &ps->pi_head);

	return (pi_handle_t)pi;

out_accepted_proposal:
	if (ps->role[myid] & ACCEPTOR)
		free(acceptor->accepted_proposal);
out_acceptor:
	if (ps->role[myid] & ACCEPTOR)
		free(acceptor);
out_proposal:
	if (ps->role[myid] & PROPOSER)
		free(proposer->proposal);
out_proposer:
	if (ps->role[myid] & PROPOSER)
		free(proposer);
out_prio:
	if (pi->prio)
		free(pi->prio);
out_pi:
	free(pi);
out:
	return rv;
}

int paxos_round_request(pi_handle_t handle,
			void *value,
			int *round,
			void (*end_request) (pi_handle_t handle,
					     int round,
					     int result))
{
	struct paxos_instance *pi = (struct paxos_instance *)handle;
	int myid = pi->ps->p_op->get_myid();
	int rv = *round;

	if (!(pi->ps->role[myid] & PROPOSER)) {
		log_debug("only proposer can do this");
		return -EOPNOTSUPP;
	}

	pi->proposer->state = PREPARING;
	pi->proposer->open_number = 0;
	pi->proposer->accepted_number = 0;
	pi->proposer->proposed = 0; 
	memcpy(pi->proposer->proposal->value, value, pi->ps->valuelen);

	pi->end = end_request;
	pi->ps->r_op->prepare(pi, &rv);

	return rv;
}

int paxos_recovery_status_get(pi_handle_t handle)
{	
	struct paxos_instance *pi = (struct paxos_instance *)handle;
	int myid = pi->ps->p_op->get_myid();

	if (!(pi->ps->role[myid] & ACCEPTOR))
		return -EOPNOTSUPP;

	if (pi->acceptor->state == RECOVERY)
		return 1;
	else
		return 0;
}

int paxos_recovery_status_set(pi_handle_t handle, int recovery)
{
	struct paxos_instance *pi = (struct paxos_instance *)handle;
	int myid = pi->ps->p_op->get_myid();

	if (!(pi->ps->role[myid] & ACCEPTOR))
		return -EOPNOTSUPP;

	if (recovery)
		pi->acceptor->state = RECOVERY;
	else
		pi->acceptor->state = INIT;

	return 0;
}

int paxos_propose(pi_handle_t handle, void *value, int round)
{
	struct paxos_instance *pi = (struct paxos_instance *)handle;
	struct paxos_msghdr *hdr;
	void *extra, *msg;
	int len = sizeof(struct paxos_msghdr)
			 + pi->ps->extralen + pi->ps->valuelen;

	if (!pi->proposer->ballot)
		pi->proposer->ballot = round;
	if (round != pi->proposer->ballot) {
		log_debug("round: %d, proposer ballot: %d",
			  round, pi->proposer->ballot);
		return -EINVAL;
	}
	msg = malloc(len);
	if (!msg) {
		log_error("no mem for msg");
		return -ENOMEM;
	}

	pi->proposer->state = PROPOSING;
	strcpy(pi->proposer->proposal->value, value);
	pi->proposer->accepted_number = 0;
	pi->round = round;

	memset(msg, 0, len);
	hdr = msg;
	hdr->state = htonl(PROPOSING);
	hdr->from = htonl(pi->ps->p_op->get_myid());
	hdr->proposer_id = hdr->from;
	strcpy(hdr->psname, pi->ps->name);
	strcpy(hdr->piname, pi->name);
	hdr->ballot_number = htonl(pi->round);
	hdr->extralen = htonl(pi->ps->extralen);
	extra = (char *)msg + sizeof(struct paxos_msghdr);
	memcpy((char *)msg + sizeof(struct paxos_msghdr) + pi->ps->extralen,
		value, pi->ps->valuelen);

	if (pi->ps->p_op->propose)
		pi->ps->p_op->propose(handle, extra, round, value);

	if (pi->ps->p_op->broadcast)
		pi->ps->p_op->broadcast(msg, len);
	else {
		int i;
		for (i = 0; i < pi->ps->number; i++) {
			if (pi->ps->role[i] & ACCEPTOR)
				pi->ps->p_op->send(i, msg, len);
		}
	}

	free(msg);
	return 0;
}

int paxos_catchup(pi_handle_t handle)
{
	struct paxos_instance *pi = (struct paxos_instance *)handle;
	
	return pi->ps->p_op->catchup(handle);
}

int paxos_recvmsg(void *msg, int msglen)
{
	struct paxos_msghdr *hdr = msg;
	struct paxos_space *ps;
	struct paxos_instance *pi;
	int found = 0;
	int myid;

	list_for_each_entry(ps, &ps_head, list) {
		if (!strcmp(ps->name, hdr->psname)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		log_error("could not find the received ps name (%s) "
			  "in registered list", hdr->psname);
		return -EINVAL;
	}
	myid = ps->p_op->get_myid();

	found = 0;
	list_for_each_entry(pi, &ps->pi_head, list) {
		if (!strcmp(pi->name, hdr->piname)) {
			found = 1;
			break;
		}
	}
	if (!found)
		paxos_instance_init((ps_handle_t)ps, hdr->piname, NULL);

	switch (ntohl(hdr->state)) {
	case PREPARING:
		if (ps->role[myid] & ACCEPTOR)
			ps->a_op->promise(ps, pi, msg, msglen);
		break;
	case PROMISING:
		ps->r_op->propose(ps, pi, msg, msglen);
		break;
	case PROPOSING:
		if (ps->role[myid] & ACCEPTOR)
			ps->a_op->accepted(ps, pi, msg, msglen);
		break;
	case ACCEPTING:
		if (ntohl(hdr->proposer_id) == myid)
			ps->r_op->commit(ps, pi, msg, msglen);
		else if (ps->role[myid] & LEARNER)
			ps->l_op->response(ps, pi, msg, msglen);
		break;
	default:
		log_debug("invalid message type: %d", ntohl(hdr->state));
		break;
	};

	return 0;
}

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

#include <string.h>
#include <stdlib.h>
#include <net/if.h>
#include <asm/types.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include "list.h"
#include "booth.h"
#include "log.h"
#include "config.h"
#include "paxos_lease.h"
#include "transport.h"

#define BOOTH_IPADDR_LEN	(sizeof(struct in6_addr))

#define NETLINK_BUFSIZE		16384
#define SOCKET_BUFFER_SIZE	160000
#define FRAME_SIZE_MAX		10000

extern struct client *client;
extern struct pollfd *pollfd;

static struct booth_node local;

struct tcp_conn {
	int s;
	struct sockaddr to;
	struct list_head list;
};

static LIST_HEAD(tcp);

struct udp_context {
	int s;
	struct iovec iov_recv;
	char iov_buffer[FRAME_SIZE_MAX];
} udp;

static int (*deliver_fn) (void *msg, int msglen);

static int ipaddr_to_sockaddr(struct booth_node *ipaddr,
		       uint16_t port,
		       struct sockaddr_storage *saddr,
		       int *addrlen)
{
	int rv = -1;

	if (ipaddr->family == AF_INET) {
		struct in_addr addr;
		struct sockaddr_in *sin = (struct sockaddr_in *)saddr;
		memset(sin, 0, sizeof(struct sockaddr_in));
		sin->sin_family = ipaddr->family;
		sin->sin_port = htons(port);
		inet_pton(AF_INET, ipaddr->addr, &addr);
		memcpy(&sin->sin_addr, &addr, sizeof(struct in_addr));
		*addrlen = sizeof(struct sockaddr_in);
		rv = 0;
	}

	if (ipaddr->family == AF_INET6) {
		struct in6_addr addr;
		struct sockaddr_in6 *sin = (struct sockaddr_in6 *)saddr;
		memset(sin, 0, sizeof(struct sockaddr_in6));
		sin->sin6_family = ipaddr->family;
		sin->sin6_port = htons(port);
		sin->sin6_scope_id = 2;
		inet_pton(AF_INET6, ipaddr->addr, &addr);
		memcpy(&sin->sin6_addr, &addr, sizeof(struct in6_addr));
		*addrlen = sizeof(struct sockaddr_in6);
		rv = 0;
	}

	return rv;
}

static void parse_rtattr(struct rtattr *tb[],
			 int max, struct rtattr *rta, int len)
{
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta,len);
	}
}

static int find_myself(struct booth_node *node)
{
	int fd, addrlen, found = 0;
	struct sockaddr_nl nladdr;
	unsigned char ndaddr[BOOTH_IPADDR_LEN];
	unsigned char ipaddr[BOOTH_IPADDR_LEN];
	static char rcvbuf[NETLINK_BUFSIZE];
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;

	memset(ipaddr, 0, BOOTH_IPADDR_LEN);
	memset(ndaddr, 0, BOOTH_IPADDR_LEN);
	if (node->family == AF_INET) {
		inet_pton(AF_INET, node->addr, ndaddr);
		addrlen = sizeof(struct in_addr);
	} else if (node->family == AF_INET6) {
		inet_pton(AF_INET6, node->addr, ndaddr);
		addrlen = sizeof(struct in6_addr);
	} else {
		log_error("invalid INET family");
		return 0;
	}

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (fd < 0) {
		log_error("failed to create netlink socket");
		return 0;
	}

	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

	memset(&nladdr, 0, sizeof(nladdr));
	nladdr.nl_family = AF_NETLINK;

	memset(&req, 0, sizeof(req));
	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = RTM_GETADDR;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = 1;
	req.g.rtgen_family = AF_INET;

	if (sendto(fd, (void *)&req, sizeof(req), 0,
		(struct sockaddr*)&nladdr, sizeof(nladdr)) < 0)  {
		close(fd);
		log_error("failed to send data to netlink socket");
		return 0;
	}

	while (1) {
		int status;
		struct nlmsghdr *h;
		struct iovec iov = { rcvbuf, sizeof(rcvbuf) };
		struct msghdr msg = {
			(void *)&nladdr, sizeof(nladdr),
			&iov,   1,
			NULL,   0,
			0
		};

		status = recvmsg(fd, &msg, 0);
		if (!status) {
			close(fd);
			log_error("failed to recvmsg from netlink socket");
			return 0;
		}

		h = (struct nlmsghdr *)rcvbuf;
		if (h->nlmsg_type == NLMSG_DONE)
			break;

		if (h->nlmsg_type == NLMSG_ERROR) {
			close(fd);
			log_error("netlink socket recvmsg error");
			return 0;
		}

		while (NLMSG_OK(h, status)) {
			if (h->nlmsg_type == RTM_NEWADDR) {
				struct ifaddrmsg *ifa = NLMSG_DATA(h);
				struct rtattr *tb[IFA_MAX+1];
				int len = h->nlmsg_len 
					  - NLMSG_LENGTH(sizeof(*ifa));

				memset(tb, 0, sizeof(tb));
				parse_rtattr(tb, IFA_MAX, IFA_RTA(ifa), len);
				memcpy(ipaddr, RTA_DATA(tb[IFA_ADDRESS]),
					BOOTH_IPADDR_LEN);
				if (!memcmp(ipaddr, ndaddr, addrlen)) {
					found = 1;
					goto out;
				}
	
			}
			h = NLMSG_NEXT(h, status);
		}
	}

out:
	close(fd);
	return found;
}

static int load_myid(void)
{
	int i;

	for (i = 0; i < booth_conf->node_count; i++) {
		if (find_myself(&booth_conf->node[i])) {
			booth_conf->node[i].local = 1;
			if (!local.family)
				memcpy(&local, &booth_conf->node[i],
				       sizeof(struct booth_node));
			return booth_conf->node[i].nodeid;
		}
	}

	return -1;
}

static int booth_get_myid(void)
{
	if (local.local)
		return local.nodeid;
	else
		return -1;
}

static void process_dead(int ci)
{
	struct tcp_conn *conn, *safe;

	list_for_each_entry_safe(conn, safe, &tcp, list) {
		if (conn->s == client[ci].fd) {
			list_del(&conn->list);
			free(conn);
			break;
		}
	}
	close(client[ci].fd);
	client[ci].workfn = NULL;
	client[ci].fd = -1;
	pollfd[ci].fd = -1;
}

static void process_tcp_listener(int ci)
{
	int fd, i, one = 1;
	socklen_t addrlen = sizeof(struct sockaddr);
	struct sockaddr addr;
	struct tcp_conn *conn;

	fd = accept(client[ci].fd, &addr, &addrlen);
	if (fd < 0) {
		log_error("process_tcp_listener: accept error %d %d",
			  fd, errno);
		return;
	}
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&one, sizeof(one));

	conn = malloc(sizeof(struct tcp_conn));
	if (!conn) {
		log_error("failed to alloc mem");
		return;
	}
	memset(conn, 0, sizeof(struct tcp_conn));
	conn->s = fd;
	memcpy(&conn->to, &addr, sizeof(struct sockaddr));
	list_add_tail(&conn->list, &tcp);

	i = client_add(fd, process_connection, process_dead);

	log_debug("client connection %d fd %d", i, fd);
}

static int setup_tcp_listener(void)
{
	struct sockaddr_storage sockaddr;
	int s, addrlen, rv;

	s = socket(local.family, SOCK_STREAM, 0);
	if (s == -1) {
		log_error("failed to create tcp socket %s", strerror(errno));
		return s;
	}

	ipaddr_to_sockaddr(&local, BOOTH_CMD_PORT, &sockaddr, &addrlen);
	rv = bind(s, (struct sockaddr *)&sockaddr, addrlen);
	if (rv == -1) {
		log_error("failed to bind socket %s", strerror(errno));
		return rv;
	}

	rv = listen(s, 5);
	if (rv == -1) {
		log_error("failed to listen on socket %s", strerror(errno));
		return rv;
	}

	return s;
}

static int booth_tcp_init(void * unused __attribute__((unused)))
{
	int rv;

	if (!local.local)
		return -1;

	rv = setup_tcp_listener();
	if (rv < 0)
		return rv;

	client_add(rv, process_tcp_listener, NULL);

	return 0;
}

static int connect_nonb(int sockfd, const struct sockaddr *saptr,
			socklen_t salen, int sec)
{
	int		flags, n, error;
	socklen_t	len;
	fd_set		rset, wset;
	struct timeval	tval;

	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	error = 0;
	if ( (n = connect(sockfd, saptr, salen)) < 0)
		if (errno != EINPROGRESS)
			return -1;

	if (n == 0)
		goto done;	/* connect completed immediately */

	FD_ZERO(&rset);
	FD_SET(sockfd, &rset);
	wset = rset;
	tval.tv_sec = sec;
	tval.tv_usec = 0;

	if ((n = select(sockfd + 1, &rset, &wset, NULL,
	    sec ? &tval : NULL)) == 0) {
		/* leave outside function to close */
		/* timeout */
		/* close(sockfd); */	
		errno = ETIMEDOUT;
		return -1;
	}

	if (FD_ISSET(sockfd, &rset) || FD_ISSET(sockfd, &wset)) {
		len = sizeof(error);
		if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
			return -1;	/* Solaris pending error */
	} else {
		log_error("select error: sockfd not set");
		return -1;
	}

done:
	fcntl(sockfd, F_SETFL, flags);	/* restore file status flags */

	if (error) {
		/* leave outside function to close */
		/* close(sockfd); */	
		errno = error;
		return -1;
	}

	return 0;
}

static int booth_tcp_open(struct booth_node *to)
{
	struct sockaddr_storage sockaddr;
	struct tcp_conn *conn;
	int addrlen, rv, s, found = 0;

	ipaddr_to_sockaddr(to, BOOTH_CMD_PORT, &sockaddr, &addrlen);
	list_for_each_entry(conn, &tcp, list) {
		if (!memcmp(&conn->to, &sockaddr, sizeof(sockaddr))) {
			found = 1;
			break;
		}
	}

	if (!found) {
		s = socket(BOOTH_PROTO_FAMILY, SOCK_STREAM, 0);
		if (s == -1)
			return -1;

		rv = connect_nonb(s, (struct sockaddr *)&sockaddr, addrlen, 10);
		if (rv == -1) {
			if( errno == ETIMEDOUT)
				log_error("connection to %s timeout", to->addr);
			else 
	                        log_error("connection to %s error %s", to->addr,
					  strerror(errno));
			close(s);
			return rv;
		}

		conn = malloc(sizeof(struct tcp_conn));
		if (!conn) {
			log_error("failed to alloc mem");
			close(s);
			return -ENOMEM;
		}
		memset(conn, 0, sizeof(struct tcp_conn));
		conn->s = s;
		memcpy(&conn->to, &sockaddr, sizeof(struct sockaddr));
		list_add_tail(&conn->list, &tcp);
	}

	return conn->s;
}

static int booth_tcp_send(unsigned long to, void *buf, int len)
{
	return do_write(to, buf, len);
}

static int booth_tcp_recv(unsigned long from, void *buf, int len)
{
	return do_read(from, buf, len);
}

static int booth_tcp_close(unsigned long s)
{
	struct tcp_conn *conn;

	list_for_each_entry(conn, &tcp, list) {
		if (conn->s == s) {
			list_del(&conn->list);
			close(s);
			free(conn);
			goto out;
		}
	}
out:
	return 0;
}

static int booth_tcp_exit(void)
{
	return 0;
}

static int setup_udp_server(void)
{
	struct sockaddr_storage sockaddr;
	int addrlen, rv;
	unsigned int recvbuf_size;

	udp.s = socket(local.family, SOCK_DGRAM, 0);
	if (udp.s == -1) {
		log_error("failed to create udp socket %s", strerror(errno));
		return -1;
	}

	rv = fcntl(udp.s, F_SETFL, O_NONBLOCK);
	if (rv == -1) {
		log_error("failed to set non-blocking operation "
			  "on udp socket: %s", strerror(errno));
		close(udp.s);
		return -1;
	}

	ipaddr_to_sockaddr(&local, booth_conf->port, &sockaddr, &addrlen);

	rv = bind(udp.s, (struct sockaddr *)&sockaddr, addrlen);
	if (rv == -1) {
		log_error("failed to bind socket %s", strerror(errno));
		close(udp.s);
		return -1;
	}

	recvbuf_size = SOCKET_BUFFER_SIZE;
	rv = setsockopt(udp.s, SOL_SOCKET, SO_RCVBUF, 
			&recvbuf_size, sizeof(recvbuf_size));
	if (rv == -1) {
		log_error("failed to set recvbuf size");
		close(udp.s);
		return -1;
	}

	return udp.s;
}

static void process_recv(int ci)
{
	struct msghdr msg_recv;
	struct sockaddr_storage system_from;
	int received;
	unsigned char *msg_offset;

	msg_recv.msg_name = &system_from;
	msg_recv.msg_namelen = sizeof (struct sockaddr_storage);
	msg_recv.msg_iov = &udp.iov_recv;
	msg_recv.msg_iovlen = 1;
	msg_recv.msg_control = 0;
	msg_recv.msg_controllen = 0;
	msg_recv.msg_flags = 0;

	received = recvmsg(client[ci].fd, &msg_recv,
			   MSG_NOSIGNAL | MSG_DONTWAIT);
	if (received == -1)
		return;

	msg_offset = udp.iov_recv.iov_base;

	deliver_fn(msg_offset, received);
}

static int booth_udp_init(void *f)
{
	int myid = -1;

	memset(&local, 0, sizeof(struct booth_node));

	myid = load_myid();
	if (myid < 0) {
		log_error("can't find myself in config file");
		return -1;
	}

	memset(&udp, 0, sizeof(struct udp_context));
	udp.iov_recv.iov_base = udp.iov_buffer;
	udp.iov_recv.iov_len = FRAME_SIZE_MAX;   

	udp.s = setup_udp_server();
	if (udp.s == -1)
		return -1;

	deliver_fn = f;

	client_add(udp.s, process_recv, NULL);

	return 0;
}

static int booth_udp_send(unsigned long to, void *buf, int len)
{
	struct msghdr msg;
	struct sockaddr_storage sockaddr;
	struct iovec iovec;
	unsigned int iov_len;
	int addrlen = 0, rv;

	iovec.iov_base = (void *)buf;
	iovec.iov_len = len;
	iov_len = 1;

	ipaddr_to_sockaddr((struct booth_node *)to, booth_conf->port,
			   &sockaddr, &addrlen);
	
	msg.msg_name = &sockaddr;
	msg.msg_namelen = addrlen;
	msg.msg_iov = (void *)&iovec;
	msg.msg_iovlen = iov_len;
	msg.msg_control = 0;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;

	rv = sendmsg(udp.s, &msg, MSG_NOSIGNAL);
	if (rv < 0)
		return rv;

	return 0;
}

static int booth_udp_broadcast(void *buf, int len)
{
	int i;

	if (!booth_conf || !booth_conf->node_count)
		return -1;

	for (i = 0; i < booth_conf->node_count; i++)
		booth_udp_send((unsigned long)&booth_conf->node[i], buf, len);
	
	return 0;
}

static int booth_udp_exit(void)
{
	return 0;
}

/* SCTP transport layer has not been developed yet */
static int booth_sctp_init(void *f __attribute__((unused)))
{
	return 0;
}

static int booth_sctp_send(unsigned long to __attribute__((unused)),
			   void *buf __attribute__((unused)),
			   int len __attribute__((unused)))
{
	return 0;
}

static int booth_sctp_broadcast(void *buf __attribute__((unused)),
				int len __attribute__((unused)))
{
	return 0;
}

static int booth_sctp_exit(void)
{
	return 0;
}

struct booth_transport booth_transport[] = {
	{
		.name = "TCP",
		.init = booth_tcp_init,
		.get_myid = booth_get_myid,
		.open = booth_tcp_open,
		.send = booth_tcp_send,
		.recv = booth_tcp_recv,
		.close = booth_tcp_close,
		.exit = booth_tcp_exit
	},
	{
		.name = "UDP",
		.init = booth_udp_init,
		.get_myid = booth_get_myid,
		.send = booth_udp_send,
		.broadcast = booth_udp_broadcast,
		.exit = booth_udp_exit
	},
	{
		.name = "SCTP",
		.init = booth_sctp_init,
		.get_myid = booth_get_myid,
		.send = booth_sctp_send,
		.broadcast = booth_sctp_broadcast,
		.exit = booth_sctp_exit
	}
};


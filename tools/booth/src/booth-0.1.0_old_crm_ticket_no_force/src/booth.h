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

#ifndef _BOOTH_H
#define _BOOTH_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BOOTH_LOG_DUMP_SIZE (1024*1024)

#define BOOTH_RUN_DIR "/var/run"
#define BOOTH_LOG_DIR "/var/log"
#define BOOTH_LOGFILE_NAME "booth.log"
#define BOOTH_DEFAULT_LOCKFILE BOOTH_RUN_DIR "/booth.pid"
#define BOOTH_DEFAULT_CONF "/etc/booth/booth.conf"

#define DAEMON_NAME		"booth"
#define BOOTH_NAME_LEN		63
#define BOOTH_PATH_LEN		127

#define BOOTHC_SOCK_PATH		"boothc_lock"
#define BOOTH_PROTO_FAMILY	AF_INET
#define BOOTH_CMD_PORT		22075

#define BOOTHC_MAGIC		0x5F1BA08C
#define BOOTHC_VERSION		0x00010000

struct boothc_header {
	uint32_t magic;
	uint32_t version;
	uint32_t cmd;
	uint32_t expiry;
	uint32_t len;
	uint32_t result;
};

typedef enum {
	BOOTHC_CMD_LIST = 1,
	BOOTHC_CMD_GRANT,
	BOOTHC_CMD_REVOKE,
	BOOTHC_CMD_CATCHUP,
} cmd_request_t;

typedef enum {
	BOOTHC_RLT_ASYNC = 1,
	BOOTHC_RLT_SYNC_SUCC,
	BOOTHC_RLT_SYNC_FAIL,
	BOOTHC_RLT_INVALID_ARG,
	BOOTHC_RLT_REMOTE_OP,
	BOOTHC_RLT_OVERGRANT,
} cmd_result_t;

struct client {
        int fd;
        void *workfn;
        void *deadfn;
};

int client_add(int fd, void (*workfn)(int ci), void (*deadfn)(int ci));
int do_read(int fd, void *buf, size_t count);
int do_write(int fd, void *buf, size_t count);
void process_connection(int ci);
void safe_copy(char *dest, char *value, size_t buflen, const char *description);

#endif /* _BOOTH_H */

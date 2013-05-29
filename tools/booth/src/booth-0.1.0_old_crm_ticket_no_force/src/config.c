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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "booth.h"
#include "config.h"
#include "ticket.h"
#include "log.h"

static int ticket_size = 0;

static int ticket_realloc(void)
{
	void *p;

	booth_conf = realloc(booth_conf, sizeof(struct booth_config)
					 + (ticket_size + TICKET_ALLOC)
					 * sizeof(struct ticket_config));
	if (!booth_conf) {
		log_error("can't alloc more booth config");
		return -ENOMEM;
	}

	p = (char *) booth_conf + sizeof(struct booth_config)
	    + ticket_size * sizeof(struct ticket_config);
	memset(p, 0, TICKET_ALLOC * sizeof(struct ticket_config));
	ticket_size += TICKET_ALLOC;

	return 0;
}

int read_config(const char *path)
{
	char line[1024];
	FILE *fp;
	char *s, *key, *val, *expiry, *weight, *c;
	int in_quotes, got_equals, got_quotes, i;
	int lineno = 0;
	int got_transport = 0;

	fp = fopen(path, "r");
	if (!fp) {
		log_error("failed to open %s: %s", path, strerror(errno));
		return -1;
	}

	booth_conf = malloc(sizeof(struct booth_config)
			  + TICKET_ALLOC * sizeof(struct ticket_config));
	if (!booth_conf) {
		log_error("failed to alloc memory for booth config");
		return -ENOMEM;
	}
	memset(booth_conf, 0, sizeof(struct booth_config)
		+ TICKET_ALLOC * sizeof(struct ticket_config));
	ticket_size = TICKET_ALLOC;

	while (fgets(line, sizeof(line), fp)) {
		lineno++;
		s = line;
		while (*s == ' ')
			s++;
		if (*s == '#' || *s == '\n')
			continue;
		if (*s == '-' || *s == '.' || *s =='/'
		    || *s == '+' || *s == '(' || *s == ')'
		    || *s == ':' || *s == ',' || *s == '@'
		    || *s == '=' || *s == '"') {
			log_error("invalid key name in config file "
				  "('%c', line %d char %ld)", *s, lineno, (long)(s - line));
			goto out;
		}
		key = s;        /* will point to the key on the left hand side    */
		val = NULL;     /* will point to the value on the right hand side */
		in_quotes = 0;  /* true iff we're inside a double-quoted string   */
		got_equals = 0; /* true iff we're on the RHS of the = assignment  */
		got_quotes = 0; /* true iff the RHS is quoted                     */
		while (*s != '\n' && *s != '\0') {
			if (!(*s >='a' && *s <= 'z')
			     && !(*s >= 'A' && *s <= 'Z')
			     && !(*s >= '0' && *s <= '9')
			     && !(*s == '_')
			     && !(*s == '-')
			     && !(*s == '.')
			     && !(*s == '/')
			     && !(*s == ' ')
			     && !(*s == '+')
			     && !(*s == '(')
			     && !(*s == ')')
			     && !(*s == ':')
			     && !(*s == ';')
			     && !(*s == ',')
			     && !(*s == '@')
			     && !(*s == '=')
			     && !(*s == '"')) {
				log_error("invalid character ('%c', line %d char %ld)"
					  " in config file", *s, lineno, (long)(s - line));
				goto out;
			}
			if (*s == '=' && !got_equals) {
				got_equals = 1;
				*s = '\0';
				val = s + 1;
			} else if ((*s == '=' || *s == '_' || *s == '-' || *s == '.')
				   && got_equals && !in_quotes) {
				log_error("invalid config file format: unquoted '%c' "
					  "(line %d char %ld)", *s, lineno, (long)(s - line));
				goto out;
			} else if ((*s == '/' || *s == '+'
				    || *s == '(' || *s == ')' || *s == ':'
				    || *s == ',' || *s == '@') && !in_quotes) {
				log_error("invalid config file format: unquoted '%c' "
					  "(line %d char %ld)", *s, lineno, (long)(s - line));
				goto out;
			} else if ((*s == ' ')
				   && !in_quotes && !got_quotes) {
				log_error("invalid config file format: unquoted whitespace "
					  "(line %d char %ld)", lineno, (long)(s - line));
				goto out;
			} else if (*s == '"' && !got_equals) {
				log_error("invalid config file format: unexpected quotes "
					  "(line %d char %ld)", lineno, (long)(s - line));
				goto out;
			} else if (*s == '"' && !in_quotes) {
				in_quotes = 1;
				if (val) {
					val++;
					got_quotes = 1;
				}
			} else if (*s == '"' && in_quotes) {
				in_quotes = 0;
				*s = '\0';
			}
			s++;		 
		}
		if (!got_equals) {
			log_error("invalid config file format: missing '=' (lineno %d)",
				  lineno);
			goto out;
		}
		if (in_quotes) {
			log_error("invalid config file format: unterminated quotes (lineno %d)",
				  lineno);
			goto out;
		}
		if (!got_quotes)
			*s = '\0';

		if (strlen(key) > BOOTH_NAME_LEN
		    || strlen(val) > BOOTH_NAME_LEN) {
			log_error("key/value too long");
			goto out;
		}

		if (!strcmp(key, "transport")) {
			if (!strcmp(val, "UDP"))
				booth_conf->proto = UDP;
			else if (!strcmp(val, "SCTP"))
				booth_conf->proto = SCTP;
			else {
				log_error("invalid transport protocol");
				goto out;
			}
			got_transport = 1;
		}

		if (!strcmp(key, "port"))
			booth_conf->port = atoi(val);

		if (!strcmp(key, "site")) {
			if (booth_conf->node_count == MAX_NODES) {
				log_error("too many nodes");
				goto out;
			}
			booth_conf->node[booth_conf->node_count].family =
				BOOTH_PROTO_FAMILY;
			booth_conf->node[booth_conf->node_count].type = SITE;
			booth_conf->node[booth_conf->node_count].nodeid = 
				booth_conf->node_count;
			strcpy(booth_conf->node[booth_conf->node_count++].addr,
				val);
		}
		
		if (!strcmp(key, "arbitrator")) {
			if (booth_conf->node_count == MAX_NODES) {
				log_error("too many nodes");
				goto out;
			}
			booth_conf->node[booth_conf->node_count].family =
				BOOTH_PROTO_FAMILY;
			booth_conf->node[booth_conf->node_count].type =
				ARBITRATOR;
			booth_conf->node[booth_conf->node_count].nodeid = 
				booth_conf->node_count;
			strcpy(booth_conf->node[booth_conf->node_count++].addr,
				val);
		}

		if (!strcmp(key, "ticket")) {
			int count = booth_conf->ticket_count;
			if (booth_conf->ticket_count == ticket_size) {
				if (ticket_realloc() < 0)
					goto out;
			}
			expiry = index(val, ';');
			weight = rindex(val, ';');
			if (!expiry) {
				strcpy(booth_conf->ticket[count].name, val);
				booth_conf->ticket[count].expiry = DEFAULT_TICKET_EXPIRY;
				log_info("expire is not set in %s."
					 " Set the default value %ds.",
					 booth_conf->ticket[count].name,
					 DEFAULT_TICKET_EXPIRY);
			}
			else if (expiry && expiry == weight) {
				*expiry++ = '\0';
				while (*expiry == ' ')
					expiry++;
				strcpy(booth_conf->ticket[count].name, val);
				booth_conf->ticket[count].expiry = atoi(expiry);
			} else {
				*expiry++ = '\0';
				*weight++ = '\0';
				while (*expiry == ' ')
					expiry++;
				while (*weight == ' ')
					weight++;
				strcpy(booth_conf->ticket[count].name, val);
				booth_conf->ticket[count].expiry = atoi(expiry);
				i = 0;
				while ((c = index(weight, ','))) {
					*c++ = '\0';
					booth_conf->ticket[count].weight[i++]
						= atoi(weight);
					while (*c == ' ')
						c++;
					weight = c;
					if (i == MAX_NODES) {
						log_error("too many weights");
						break;
					}
				}
			}
			booth_conf->ticket_count++;
		}
	}

	if (!got_transport) {
		log_error("config file was missing transport line");
		goto out;
	}

	return 0;

out:
	free(booth_conf);
	return -1;
}

int check_config(int type)
{
//	int i;

	if (!booth_conf)
		return -1;

/*	for (i = 0; i < booth_conf->node_count; i++) {
		if (booth_conf->node[i].local && booth_conf->node[i].type ==
			type)
			return 0;
	}

	return -1;*/
	return 0;
}

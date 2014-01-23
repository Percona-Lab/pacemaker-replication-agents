/*
 * ybinlogp: A mysql binary log parser and query tool
 *
 * (C) 2010-2011 Yelp, Inc.
 *
 * This work is licensed under the ISC/OpenBSD License. The full
 * contents of that license can be found under license.txt
 * 
 * Modified by Yves Trudeau, 2013-03 to get Xid, pos and md5 in
 * order to be able to relate relay-log Xid with binlog when the 
 * host is running with log_slave_updates.  Useful for the Percona
 * replication monitor
 * 
 */

#define _XOPEN_SOURCE 600
#define _GNU_SOURCE

#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "debugs.h"
#include "ybinlogp.h"
#include <openssl/md5.h>

#define event_data_len(e) (e->length - EVENT_HEADER_SIZE)

void usage(void) {
	fprintf(stderr, "ybinlogp_test [options] binlog\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options\n");
	fprintf(stderr, "\t-h           show this help\n");
	fprintf(stderr, "\t-E           do not enforce server-id checking\n");
	fprintf(stderr, "\t-o OFFSET    find the first event after the given offset\n");
	fprintf(stderr, "\t-t TIME      find the first event after the given time\n");
	fprintf(stderr, "\t-a COUNT     When used with one of the above, print COUNT items after the first one, default 2\n");
	fprintf(stderr, "\t\t\t\tAccepts either an integer or the text 'all'\n");
	fprintf(stderr, "\t-D DBNAME    Filter query events that were not in DBNAME\n");
	fprintf(stderr, "\t\t\t\tNote that this still shows transaction control events\n");
	fprintf(stderr, "\t\t\t\tsince those do not have an associated database. Mea culpa.\n");
	fprintf(stderr, "\t-q           Be quieter\n");
}

int main(int argc, char** argv) {
	int opt;
	int fd;
	struct ybp_binlog_parser* bp;
	struct ybp_event* evbuf;
	off64_t starting_offset = -1;
	int num_to_show = 2;
	int show_all = true;
	bool q_mode = true;
	bool esi = true;
	char* database_limit = NULL;
	int debug = 0;
    
	while ((opt = getopt(argc, argv, "ho:")) != -1) {
		switch (opt) {
			case 'h':
				usage();
				return 0;
			case 'o':      /* Offset mode */
				starting_offset = strtoll(optarg,NULL,10);
				break;
			case '?':
				fprintf(stderr, "Unknown argument %c\n", optopt);
				usage();
				return 1;
				break;
		}
	}
	if (optind >= argc) {
		usage();
		return 2;
	}
	if ((fd = open(argv[optind], O_RDONLY|O_LARGEFILE)) <= 0) {
		perror("Error opening file");
		return 1;
	}
	if ((bp = ybp_get_binlog_parser(fd)) == NULL) {
		perror("init_binlog_parser");
		return 1;
	}
	bp->enforce_server_id = esi;
	if ((evbuf = malloc(sizeof(struct ybp_event))) == NULL) {
		perror("malloc event");
		return 1;
	}
	ybp_init_event(evbuf);
	int i = 0;
	struct ybp_xid_event* x;
	struct ybp_query_event_safe* s;
	MD5_CTX c; 
	unsigned int n;
	unsigned char out[MD5_DIGEST_LENGTH];
	char* cptr;
	uint32_t next_position=4;
	MD5_Init(&c);
	while ((ybp_next_event(bp, evbuf) >= 0) && (show_all || i < num_to_show)) {
	    if (q_mode) {
	    	if (debug) printf("type_code: %i pos=%llu\n",evbuf->type_code,(long long unsigned int) evbuf->offset);
		if (starting_offset > 0 && starting_offset > evbuf->offset) { 
			if (debug) printf("continuing %llu > %llu\n",(long long unsigned int) starting_offset, (long long unsigned int) evbuf->offset); 
			next_position=evbuf->offset;
			continue; 
		}
            	switch (evbuf->type_code) {
		              
                case QUERY_EVENT:
                    s = ybp_event_to_safe_qe(evbuf);
                    MD5_Update(&c, &evbuf->timestamp, 4); if (debug) { cptr= (char *) &evbuf->timestamp; for(n=0; n<4; n++)  printf("%hhX", cptr[n]); }
                    MD5_Update(&c, s->statement, s->statement_len); if (debug) { printf(" | ");cptr=s->statement; for(n=0; n < s->statement_len; n++)  printf("%hhX", cptr[n]); printf(" "); }
                    ybp_dispose_safe_qe(s);
                    break;
        
                case WRITE_ROWS_EVENT:
                case UPDATE_ROWS_EVENT:
                case DELETE_ROWS_EVENT:
		    if (debug) { printf("ts=%d type=%i len=%d\n", evbuf->timestamp, evbuf->type_code, event_data_len(evbuf)); }
                    cptr = evbuf->data + 4; /* skipping table id not consistent across node */ 
                    MD5_Update(&c, cptr, event_data_len(evbuf) - 4); if (debug) { for(n=0; n < event_data_len(evbuf) - 4; n++)  printf("%hhX", cptr[n]); printf(" "); }
                    break;
                            
                case INTVAR_EVENT:
                case RAND_EVENT:
                case USER_VAR_EVENT:
                case BEGIN_LOAD_QUERY_EVENT:
                case APPEND_BLOCK_EVENT:
                case EXECUTE_LOAD_QUERY_EVENT:
                case DELETE_FILE_EVENT:
			if (debug) { printf("ts=%d type=%i len=%d\n", evbuf->timestamp, evbuf->type_code, event_data_len(evbuf)); }
                    MD5_Update(&c, evbuf->data, event_data_len(evbuf)); if (debug) { cptr=evbuf->data; for(n=0; n < event_data_len(evbuf); n++)  printf("%hhX", cptr[n]); printf(" ");}
                    break;
                    
                case XID_EVENT:
		    if (!(starting_offset > 0 && next_position == 4)) {
                    	    x = ybp_event_to_safe_xe(evbuf);
	                    /* printf("ts=%d XID=%llu pos=%llu md5=", evbuf->timestamp, (long long unsigned)x->id,(long long unsigned) next_position);*/
	                    printf("%llu,",(long long unsigned) next_position);
	                    next_position = evbuf->offset + evbuf->length;
        	            MD5_Final(out, &c);
                	    for(n=0; n<MD5_DIGEST_LENGTH; n++)  printf("%hhX", out[n]);
	                    printf("\n");
        	            MD5_Init(&c);
                    	    ybp_dispose_safe_xe(x);
		    }
                    break;
                    
                default:
                    break;
			}
            
		} else {
			ybp_print_event(evbuf, bp, stdout, q_mode, false, database_limit);
			fprintf(stdout, "\n");
		}
		ybp_reset_event(evbuf);
		i+=1;
	}
	ybp_dispose_event(evbuf);
	ybp_dispose_binlog_parser(bp);
}

/* vim: set sts=0 sw=4 ts=4 noexpandtab: */

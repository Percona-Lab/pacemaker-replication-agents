SOURCES := $(wildcard *.c *.h)
TARGETS := prm_binlog_parser

prefix := /usr

CFLAGS += -Wall -ggdb -Wextra --std=c99 -pedantic
LDFLAGS += -L. -lssl -lcrypto
#CFLAGS=-Wall -ggdb -Wextra -DDEBUG

all: $(TARGETS)

#ybinlogp: ybinlogp.o libybinlogp.so
#	gcc $(CFLAGS) $(LDFLAGS) -o $@ -lybinlogp $<

#prm_binlog_parser: prm_binlog_parser.o libybinlogp.so
#	gcc $(CFLAGS) $(LDFLAGS) -o $@ -lybinlogp $<
prm_binlog_parser: prm_binlog_parser.c libybinlogp.c ybinlogp.h
	gcc $(CFLAGS) prm_binlog_parser.c libybinlogp.c -o $@ $(LDFLAGS)

ybinlogp: ybinlogp.c libybinlogp.c ybinlogp.h
	gcc $(CFLAGS) ybinlogp.c libybinlogp.c -o $@ $(LDFLAGS)

#libybinlogp.so: libybinlogp.so.1
#	ln -fs $< $@

#libybinlogp.so.1: libybinlogp.o
#	gcc $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,$@ -o $@ $^

#libybinlogp.o: libybinlogp.c ybinlogp-private.h
#	gcc $(CFLAGS) $(LDFLAGS) -c -fPIC -o $@ $<

clean::
	rm -f $(TARGETS) *.o

#ybinlogp.o: ybinlogp.c ybinlogp.h

#prm_binlog_parser.o: prm_binlog_parser.c ybinlogp.h

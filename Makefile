CFLAGS = -D_REENTRANT -Wall -pedantic -Isrc

OSNAME = $(shell uname)
ifeq ($(OSNAME), Linux)
LDLIBS = -lpthread -lm -lrt
else
LDFLAGS = -lpthread -lm
endif

CFLAGS += -g
LDFLAGS += -g
ifdef DEBUG
endif

TARGETS = tests/log tests/chan tests/list tests/api tests/demo_pool_easy tests/demo_pool_auto_add_del_thread \
		  tests/hash tests/pool_echo_serve

all: $(TARGETS)

tests/log: tests/log.o src/threadpool.o
tests/chan: tests/chan.o src/threadpool.o
tests/demo_pool_easy: tests/demo_pool_easy.o src/threadpool.o
tests/demo_pool_auto_add_del_thread: tests/demo_pool_auto_add_del_thread.o src/threadpool.o
tests/pool_echo_serve: tests/pool_echo_serve.o src/threadpool.o
tests/list: tests/list.o src/threadpool.o
tests/hash: tests/hash.o src/threadpool.o
tests/api:  tests/api.o src/threadpool.o

src/.o: src/threadpool.c src/threadpool.h

src/threadpool.o: src/threadpool.c src/threadpool.h
	    $(CC) -c ${CFLAGS} -o $@ $<
clean:
	rm -f $(TARGETS) *~ */*~ */*.o

test:
	./tests/log
	./tests/list
	./tests/hash
	./tests/chan
	./tests/api


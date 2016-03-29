CFLAGS = -D_REENTRANT -Wall -pedantic -Isrc
LDLIBS = -lpthread -lm -lrt

CFLAGS += -g
LDFLAGS += -g
ifdef DEBUG
endif

TARGETS = tests/log tests/chan tests/list tests/api tests/pool_easy tests/pool_auto_add_del_thread \
		  tests/hash tests/pool_echo_serve

all: $(TARGETS)

tests/log: tests/log.o src/threadpool.o
tests/chan: tests/chan.o src/threadpool.o
tests/pool_easy: tests/pool_easy.o src/threadpool.o
tests/pool_auto_add_del_thread: tests/pool_auto_add_del_thread.o src/threadpool.o
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


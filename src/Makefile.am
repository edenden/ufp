bin_PROGRAMS = ufp
ufp_LDFLAGS = -lpthread -L../lib -Wl,-rpath -Wl,$(libdir)
ufp_CFLAGS = -I../lib/include
ufp_DEPENDENCIES = ../lib/libufp.la
ufp_SOURCES = main.c thread.c forward.c epoll.c netlink.c fib.c neigh.c lpm.c hash.c
ufp_LDADD = -lufp

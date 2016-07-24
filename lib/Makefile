CC = gcc
CFLAGS = -fPIC -Wall -O2 -g -I./driver -I./
LDFLAGS = -shared -Wl,-soname=libufp.so.1 # linking flags
TARGET_LIB = libufp.so.1.0.0
LIBDIR = /usr/local/lib/

SRCS = lib_main.c lib_api.c lib_mem.c lib_rtx.c \
lib_ixgbevf.c lib_ixgbevf_mac.c lib_ixgbevf_mbx.c
OBJS = $(subst .c,.o,$(SRCS))
VPATH = driver

${TARGET_LIB}: ${OBJS}
	$(CC) ${LDFLAGS} -o $@ $^
	cp $@ ${LIBDIR}
	ldconfig
	ln -s ${LIBDIR}${TARGET_LIB} ${LIBDIR}libufp.so

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET_LIB} ${OBJS} ${LIBDIR}libufp.*
	ldconfig

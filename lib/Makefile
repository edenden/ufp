CC = gcc
CFLAGS = -fPIC -Wall -O2 -g -I./
LDFLAGS = -shared -ldl -Wl,--export-dynamic,-soname=libufp.so.1
TARGET_LIB = libufp.so.1.0.0
LIBDIR = /usr/local/lib/

SRCS = lib_main.c lib_api.c lib_mem.c lib_io.c lib_dev.c
OBJS = $(subst .c,.o,$(SRCS))

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

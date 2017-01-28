CC = gcc
CFLAGS = -fPIC -Wall -O2 -g -I../../lib/
LDFLAGS = -shared
TARGET_LIB = libufp_i40e.so

SRCS = i40e_aq.c i40e_aqc.c i40e_hmc.c i40e_io.c i40e_main.c i40e_ops.c
OBJS = $(subst .c,.o,$(SRCS))

${TARGET_LIB}: ${OBJS}
	$(CC) ${LDFLAGS} -o $@ $^

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET_LIB} ${OBJS}

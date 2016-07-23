SRCS = lib_main.c lib_api.c lib_mem.c lib_rtx.c \
driver/lib_ixgbevf.c driver/lib_ixgbevf_mac.c driver/lib_ixgbevf_mbx.c
OBJS = $(SRCS:.c=.o)
TARGET = libufp.so
CXX = gcc
CXXFLAGS = -O3 -Wall -fPIC -I./driver/
LDFLAGS = -shared
$LIBS = 

all: $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)
	@echo done.

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -c

clean:
	rm -rf *.o $(TARGET)

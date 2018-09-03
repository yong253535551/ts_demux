CC= gcc
CXX = g++
AR = ar
RANLIB = ranlib

INCLUDES = -I./include -I/usr/local/include
LDFLAGS = -L./lib -L/usr/local/lib -L./libmpeg/debug.linux/
LIBS += -lpthread
LIBS += -lm
LIBS += -lmpeg

SRCS += source/ts_demux.cpp
SRCS += source/udp_client.cpp
SRCS += source/circular_buffer.cpp

TARGET = ts_demux

OBJS = $(addsuffix .o, $(basename $(SRCS)))

CFLAGS = $(INCLUDES) $(LDFLAGS) $(MYDEFS)

$(TARGET):
	make -C libmpeg
	$(CXX) $(SRCS) -o $@ $(CFLAGS) $(LIBS)
    
clean:
	make -C libmpeg clean
	rm -rf source/*.o *.aac *.h264 $(TARGET)

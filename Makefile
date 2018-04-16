ifeq ($(DEB_TARGET_ARCH),armel)
CROSS_COMPILE=arm-linux-gnueabi-
endif

CXX=$(CROSS_COMPILE)g++
CXX_PATH := $(shell which $(CROSS_COMPILE)g++-4.7)

CC=$(CROSS_COMPILE)gcc
CC_PATH := $(shell which $(CROSS_COMPILE)gcc-4.7)

ifneq ($(CXX_PATH),)
	CXX=$(CROSS_COMPILE)g++-4.7
endif

ifneq ($(CC_PATH),)
	CC=$(CROSS_COMPILE)gcc-4.7
endif

#CFLAGS=-Wall -ggdb -std=c++0x -O0 -I.
CFLAGS=-Wall -std=c++0x -Os -I.
LDFLAGS= -lmosquittopp -lmosquitto -ljsoncpp -lwbmqtt1

W1_BIN=wb-homa-w1

# W1
W1_H=sysfs_w1.h
W1_D=onewire_driver.h

W1_SOURCES= 					\
			sysfs_w1.cpp		\
			onewire_driver.cpp 	\
			exceptions.cpp		\
			log.cpp				\

W1_OBJECTS=$(W1_SOURCES:.cpp=.o)

all : $(W1_BIN)

# W1
%.o : %.cpp
	${CXX} -c $< -o $@ ${CFLAGS}

$(W1_BIN) : main.o $(W1_OBJECTS)
	${CXX} $^ ${LDFLAGS} -o $@


clean :
	-rm -f *.o $(W1_BIN)



install: all
	install -d $(DESTDIR)
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/lib

	install -m 0755  $(W1_BIN) $(DESTDIR)/usr/bin/$(W1_BIN)

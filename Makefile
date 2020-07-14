ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

CXXFLAGS=-Wall -std=c++14 -Os -I.

W1_SOURCES= 					\
			sysfs_w1.cpp		\
			onewire_driver.cpp 	\
			file_utils.cpp		\
			threaded_runner.cpp \

W1_OBJECTS=$(W1_SOURCES:.cpp=.o)
W1_BIN=wb-mqtt-w1
W1_LIBS= -lwbmqtt1 -lpthread

W1_TEST_SOURCES= 							\
			$(TEST_DIR)/test_main.cpp		\
			$(TEST_DIR)/sysfs_w1_test.cpp	\
			$(TEST_DIR)/onewire_driver_test.cpp	\
			
TEST_DIR=test
export TEST_DIR_ABS = $(shell pwd)/$(TEST_DIR)

W1_TEST_OBJECTS=$(W1_TEST_SOURCES:.cpp=.o)
TEST_BIN=wb-mqtt-w1-test
TEST_LIBS=-lgtest -lwbmqtt_test_utils



all : $(W1_BIN)

# W1
%.o : %.cpp
	${CXX} -c $< -o $@ ${CXXFLAGS}

$(W1_BIN) : main.o $(W1_OBJECTS)
	${CXX} $^ ${W1_LIBS} -o $@

$(TEST_DIR)/$(TEST_BIN): $(W1_OBJECTS) $(W1_TEST_OBJECTS)
	${CXX} $^ $(W1_LIBS) $(TEST_LIBS) -o $@ -fno-lto

test: $(TEST_DIR)/$(TEST_BIN)
	rm -f $(TEST_DIR)/*.dat.out
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind --error-exitcode=180 -q $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		else $(TEST_DIR)/abt.sh show; exit 1; fi; \
    else \
        $(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || { $(TEST_DIR)/abt.sh show; exit 1; } \
	fi

clean :
	-rm -f *.o $(W1_BIN)
	-rm -f $(TEST_DIR)/*.o $(TEST_DIR)/$(TEST_BIN)


install: all
	install -D -m 0755  $(W1_BIN) $(DESTDIR)/usr/bin/$(W1_BIN)

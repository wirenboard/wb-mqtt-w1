ifneq ($(DEB_HOST_MULTIARCH),)
	CROSS_COMPILE ?= $(DEB_HOST_MULTIARCH)-
endif

ifeq ($(origin CC),default)
	CC := $(CROSS_COMPILE)gcc
endif
ifeq ($(origin CXX),default)
	CXX := $(CROSS_COMPILE)g++
endif

ifeq ($(DEBUG),)
	BUILD_DIR ?= build/release
else
	BUILD_DIR ?= build/debug
endif

PREFIX = /usr

W1_BIN = wb-mqtt-w1
SRC_DIR = src

W1_SOURCES = $(shell find $(SRC_DIR) -name *.cpp -and -not -name main.cpp)
W1_OBJECTS = $(W1_SOURCES:%=$(BUILD_DIR)/%.o)

CXXFLAGS = -Wall -std=c++14 -I$(SRC_DIR)
LDFLAGS = -lwbmqtt1 -lpthread

ifeq ($(DEBUG), 1)
	CXXFLAGS += -O0 -ggdb -rdynamic -fprofile-arcs -ftest-coverage
	LDFLAGS += -lgcov
else
	CXXFLAGS += -Os -DNDEBUG
endif

TEST_DIR = test
W1_TEST_SOURCES = $(shell find $(TEST_DIR) -name *.cpp)
W1_TEST_OBJECTS = $(W1_TEST_SOURCES:%=$(BUILD_DIR)/%.o)
TEST_BIN = wb-mqtt-w1-test
TEST_LIBS = -lgtest -lwbmqtt_test_utils

export TEST_DIR_ABS = $(CURDIR)/$(TEST_DIR)

all: $(W1_BIN)

# W1
$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(W1_BIN): $(W1_OBJECTS) $(BUILD_DIR)/src/main.cpp.o
	$(CXX) $^ $(LDFLAGS) -o $(BUILD_DIR)/$@

$(BUILD_DIR)/test/%.o: test/%.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN): $(W1_OBJECTS) $(W1_TEST_OBJECTS)
	$(CXX) $^ $(LDFLAGS) $(TEST_LIBS) -o $@ -fno-lto

test: $(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN)
	rm -f $(TEST_DIR)/*.dat.out
	if [ "$(shell arch)" != "armv7l" ] && [ "$(CROSS_COMPILE)" = "" ] || [ "$(CROSS_COMPILE)" = "x86_64-linux-gnu-" ]; then \
		valgrind --error-exitcode=180 -q $(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || \
		if [ $$? = 180 ]; then \
			echo "*** VALGRIND DETECTED ERRORS ***" 1>& 2; \
			exit 1; \
		else $(TEST_DIR)/abt.sh show; exit 1; fi; \
    else \
        $(BUILD_DIR)/$(TEST_DIR)/$(TEST_BIN) $(TEST_ARGS) || { $(TEST_DIR)/abt.sh show; exit 1; } \
	fi

lcov: test
ifeq ($(DEBUG), 1)
	geninfo --no-external -b . -o $(BUILD_DIR)/coverage.info $(BUILD_DIR)/src
	genhtml $(BUILD_DIR)/coverage.info -o $(BUILD_DIR)/cov_html
endif

clean:
	-rm -fr build

install: all
	install -Dm0755 $(BUILD_DIR)/$(W1_BIN) -t $(DESTDIR)$(PREFIX)/bin

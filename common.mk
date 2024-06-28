CC = gcc-11

ARCH := $(shell uname -m)

CFLAGS := -std=c17 -Wmissing-declarations -Wall -Wextra -MMD -fPIC -D_GNU_SOURCE
ifeq ($(ARCH), x86_64)
	MARCH ?= broadwell
	CFLAGS += -march=$(MARCH)
else ifeq ($(ARCH), aarch64)
	CFLAGS += -D__ARM_NEON__
endif

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -DNDEBUG
endif

LDLIBS=


%: %.cpp   # delete implicit rule
%: %.cpp   # add implicit rule
	$(CXX) -o $@ $(filter %.cpp %.o %.a, $^) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) $(LOADLIBS) $(LDLIBS) 


%: %.c
%: %.c
	$(CC) -o $@ $(filter %.c %.o %.a, $^) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(LOADLIBS) $(LDLIBS)

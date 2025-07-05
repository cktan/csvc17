CFLAGS = -std=c17 -fpic -Wmissing-declarations -Wall -Wextra -MMD 

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -DNDEBUG
endif

CXXFLAGS = $(subst -std=c17,-std=c++20,$(CFLAGS))
EXECS = driver

ARCH := $(shell uname -m)
ifeq ($(ARCH), x86_64)
    # Safe for AVX2 cpu (will auto-use avx512bw where available)
    CFLAGS += -mavx2 -mno-avx512f
else ifeq ($(ARCH),aarch64)
    $(error Fill this in later)
else
    $(error Unsupported CPU $(ARCH))
endif


all: $(EXECS)

driver: driver.cpp 
	$(CXX) $(CXXFLAGS) -o $@ $@.cpp 

test: all
	./driver

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch]pp

-include $(EXECS:%=%.d)

.PHONY: all clean format test

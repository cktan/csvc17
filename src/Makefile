HFILES = csvc17.h
CFILES = csvc17.c
OBJ = $(CFILES:.c=.o)

CFLAGS = -std=c17 -fpic -Wmissing-declarations -Wall -Wextra -MMD 
LIB_VERSION = 1.0
LIB = libcsvc17.a
LIB_SHARED = libcsvc17.so.$(LIB_VERSION)
EXEC =

ARCH := $(shell uname -m)
ifeq ($(ARCH), x86_64)
    # Safe for AVX2 cpu (will auto-use avx512bw where available)
    CFLAGS += -mavx2 -mno-avx512f
else ifeq ($(ARCH),aarch64)
    $(error Fill this in later)
    CFLAGS += -march=armv8-a
else
    $(error Unsupported CPU $(ARCH))
endif

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -DNDEBUG
endif

all: $(EXEC) $(LIB) $(LIB_SHARED) 

*.o: $(HFILES)

libcsvc17.a: $(OBJ)
	ar -rcs $@ $^

$(LIB_SHARED): $(OBJ)
	$(CC) -shared -o $@ $^

-include $(OBJ:%.o=%.d) $(EXEC:%=%.d)

test: all

clean:
	rm -f *.o *.d *.a *.so.* $(EXEC)

distclean: clean

format:
	clang-format -i *.[ch] 


.PHONY: all test clean distclean format


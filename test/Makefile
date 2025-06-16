CFLAGS = -std=c17 -fpic -Wmissing-declarations -Wall -Wextra -MMD
EXEC = csv2py

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -DNDEBUG
endif

all: $(EXEC)

test: all

csv2py: csv2py.c ../src/libcsvc17.a
	$(CC) $(CFLAGS) -o $@ $@.c -L../src -lcsvc17

-include $(EXEC:%=%.d)

clean:
	rm -f *.o *.d $(EXEC)

distclean: clean

format:
	clang-format -i *.[ch]

.PHONY: all clean distclean format test

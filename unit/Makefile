CFLAGS = -std=c17 -fpic -Wmissing-declarations -Wall -Wextra -MMD 
EXEC = simple simplecpp

ifdef DEBUG
    CFLAGS += -O0 -g
else
    CFLAGS += -O3 -DNDEBUG
endif

CXXFLAGS = $(subst -std=c17,-std=c++20,$(CFLAGS))
EXECS = driver

all: $(EXECS)

driver: driver.cpp ../src/libcsvc17.a
	$(CXX) $(CXXFLAGS) -o $@ $@.cpp -L ../src -lcsvc17

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch]pp

-include $(EXECS:%=%.d)

.PHONY: all clean format

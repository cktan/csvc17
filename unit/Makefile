TOPDIR = ..
HFILES =

EXECS = test

all: $(EXECS)

$(EXECS): test.cpp ../src/libcsv.a

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch] *.[ch]pp

include $(TOPDIR)/common.mk

-include $(EXECS:%=%.d)

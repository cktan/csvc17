TOPDIR = ..
HFILES =

EXECS = main

all: $(EXECS)

$(EXECS): main.cpp ../src/libcsv.a

clean:
	rm -f *.o *.a *.d $(EXECS)

format:
	clang-format -i *.[ch]pp

include $(TOPDIR)/common.mk

-include $(EXECS:%=%.d)

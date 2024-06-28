TOPDIR = ..
CFILES = csv.c
OBJ = $(CFILES:.c=.o)

all: libcsv.a

libcsv.a: $(OBJ)
	ar -rcs $@ $^

clean:
	rm -f *.o *.a *.d

format:
	clang-format -i *.c *.h 

include $(TOPDIR)/common.mk

-include $(OBJ:%.o=%.d)

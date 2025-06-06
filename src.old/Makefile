TOPDIR = ..
CFILES = csv.c
OBJ = $(CFILES:.c=.o)
EXEC = csv2py

all: libcsv.a $(EXEC)

libcsv.a: $(OBJ)
	ar -rcs $@ $^

$(EXEC): libcsv.a

clean:
	rm -f *.o *.a *.d

format:
	clang-format -i *.c *.h 

include $(TOPDIR)/common.mk

-include $(OBJ:%.o=%.d)

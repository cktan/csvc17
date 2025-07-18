.NOTPARALLEL:

prefix ?= /usr/local
# remove trailing /
override prefix := $(prefix:%/=%)
DIRS = src unit test

BUILDDIRS = $(DIRS:%=build-%)
CLEANDIRS = $(DIRS:%=clean-%)
FORMATDIRS = $(DIRS:%=format-%)
TESTDIRS = $(DIRS:%=test-%)


###################################

# Define PCFILE content based on prefix
define PCFILE
Name: libcsvc17
URL: https://github.com/cktan/csvc17/
Description: CSV Parser in C17.
Version: v1.0
Libs: -L${prefix}/lib -lcsvc17
Cflags: -I${prefix}/include
endef

# Make it available to subshells
export PCFILE

#################################

all: $(BUILDDIRS)

$(BUILDDIRS):
	$(MAKE) -C $(@:build-%=%)

install: all
	install -d ${prefix}/include
	install -d ${prefix}/lib
	install -d ${prefix}/lib/pkgconfig
	install -m 0644 -t ${prefix}/include src/csvc17.h
	install -m 0644 -t ${prefix}/include src/csv.hpp
	install -m 0644 -t ${prefix}/lib src/libcsvc17.a
	@echo "$$PCFILE" >> ${prefix}/lib/pkgconfig/csvc17.pc

test: $(TESTDIRS)

format: $(FORMATDIRS)

clean: $(CLEANDIRS)

$(TESTDIRS):
	$(MAKE) -C $(@:test-%=%) test

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

$(FORMATDIRS):
	$(MAKE) -C $(@:format-%=%) format

.PHONY: $(DIRS) $(BUILDDIRS) $(TESTDIRS) $(CLEANDIRS) $(FORMATDIRS)
.PHONY: all install test format clean

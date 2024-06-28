.NOTPARALLEL:

prefix ?= /usr/local

DIRS = src 

BUILDDIRS = $(DIRS:%=build-%)
CLEANDIRS = $(DIRS:%=clean-%)
FORMATDIRS = $(DIRS:%=format-%)

all: $(BUILDDIRS)

$(DIRS): $(BUILDDIRS)

$(BUILDDIRS):
	echo make $(@:build-%=%)
	$(MAKE) -C $(@:build-%=%)

install: all
	install -d ${prefix} ${prefix}/include ${prefix}/lib
	install -m 0644 -t ${prefix}/include/csv.h
	install -m 0644 -t ${prefix}/lib src/libcsv.a

format: $(FORMATDIRS)

clean: $(CLEANDIRS)
	rm -f $(VERSION)

$(CLEANDIRS):
	$(MAKE) -C $(@:clean-%=%) clean

$(FORMATDIRS):
	$(MAKE) -C $(@:format-%=%) format

.PHONY: $(DIRS) $(BUILDDIRS) $(CLEANDIRS) $(FORMATDIRS)
.PHONY: all install format

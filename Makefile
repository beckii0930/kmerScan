all: kmerScan
CC=gcc

DEBUG?=""
ifneq ($(DEBUG), "")
CCOPTS=$(CCOPTS_BASE) $(DEBUG)
else
CCOPTS=-O3 $(CCOPTS_BASE)
endif
STATIC=-static
ifeq ($(OPT), "1")
CCOPTS=-g $(CCOPTS_BASE) -lprofiler
STATIC=
endif

readBam: readBam.c htslib/libhts.so
	$(CC) -I htslib $^ -o $@  -L htslib/htslib -lpthread -lz -Wl,-rpath,$(PWD)/htslib
# 	g++ -I htslib $(CCOPTS) $^ -o $@  -L htslib -lhts -lpthread -lz -Wl,-rpath,$(PWD)/htslib
# 	g++ -I htslib $(CCOPTS) $^ -o $@  -L htslib -lhts -lpthread -lz -Wl,-rpath,$(PWD)/htslib

kmerScan: kmerScan.c htslib/libhts.so
#kmerScan: kmerScan.c htslib-1.11/libhts.dylib
# 	$(CC) -I htslib $(CCOPTS) $^ -o $@  -L htslib -lhts -lpthread -lz -Wl,-rpath,$(PWD)/htslib
# 	$(CC) -I htslib $^ -o $@  -L htslib -lhts -lpthread -lz -Wl,-rpath,$(PWD)/htslib
	$(CC) $(CCOPTS) -I htslib $^ -o $@  -L htslib/htslib -lpthread -lz -Wl,-rpath,$(PWD)/htslib

# the libhts.xx extension depends on the system, it can be libhts.so, a shared object file
#htslib-1.11/libhts.dylib:
htslib/libhts.so:
#	cd htslib-1.11  && autoheader && autoconf && ./configure --prefix=$(PWD) && make
	cd htslib && ./configure --prefix=$(PWD) && make

clean:
	rm -f kmerScan

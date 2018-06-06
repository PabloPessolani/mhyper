# Makefile for the tests.

CFLAGS= -O -D_MINIX -D_POSIX_SOURCE
CC =	exec cc
LDFLAGS = -i
LIBS = -lsys

OBJ= loadvmimg.o libproxy.o floppy.o at_wini.o paraprinter.o

all: loadvmimg
install: loadvmimg

loadvmimg: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
	install -S 8kw $@
	
clean:	
	-rm -rf *.o *.s *.bak loadvmimg
	
loadvmimg:	loadvmimg.c libproxy.h glo.h
libproxy:	libproxy.c libproxy.h glo.h
floppy:		floppy.c libproxy.h glo.h
at_wini:	at_wini.c libproxy.h glo.h
paraprinter: paraprinter.c libproxy.h glo.h

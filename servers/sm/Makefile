# Makefile for Security Monitor (SM)
SERVER = sm

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
UTIL_LIBS = -lsys 
LIBS = -lsys -lsysutil 

OBJ = sm.o 

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)

# install with other servers
install: /usr/sbin/$(SERVER)
/usr/sbin/$(SERVER):	$(SERVER)
	install -o root -c $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend


# Makefile for Virtual Machine Manager(VMM)
SERVER = vmm

# directories
u = /usr
i = $u/include
s = $i/sys
m = $i/minix
b = $i/ibm
k = $u/src/kernel
p = $u/src/servers/pm
f = $u/src/servers/fs

# programs, flags, etc.
CC =	exec cc
CFLAGS = -I$i
LDFLAGS = -i
LIBS = -lsys -lsysutil 

OBJ = main.o table.o utility.o vmm_test.o

# build local binary
all build:	$(SERVER)
$(SERVER):	$(OBJ)
	$(CC) -o $@ $(LDFLAGS) $(OBJ) $(LIBS)
	install -S 16k $@

# install with other servers
install: $(SERVER)
	install -o root -c $? /sbin/$(SERVER)
#	install -o root -cs $? $@

# clean up local files
clean:
	rm -f $(SERVER) *.o *.bak 

depend: 
	/usr/bin/mkdep "$(CC) -E $(CPPFLAGS)" *.c > .depend

# Include generated dependencies.
include .depend

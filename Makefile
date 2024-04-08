# Micheal Lammens, xlw945, 11335630

.PHONY: all clean

CC = gcc
CFLAGS = -g
CPPFLAGS = -std=gnu99 -Wall -pedantic -Wextra

Targets = emulate client server

PPC_COMPILE = powerpc-linux-gnu-gcc-10
ARM_COMPILE = arm-linux-gnueabihf-gcc-10

all: ${Targets}



#Emulate
emulate: emulate.o liblist.a
	${CC} ${CFLAGS} -o emulate emulate.o -L./ -llist -lm

emulate.o: emulator.c protocol.h 
	${CC} ${CFLAGS} ${CPPFLAGS} -I. -c emulator.c -o emulate.o 

client: client.o liblist.a
	${CC} ${CFLAGS} -o client client.o -L./ -llist -lm
client.o: client.c
	${CC} ${CFLAGS} ${CPPFLAGS} -I. -c client.c -o client.o
server: server.o
	${CC} ${CFLAGS} -o server server.o -lm
server.o: server.c protocol.h
	${CC} ${CFLAGS} ${CPPFLAGS} -I. -c server.c -o server.o


# list
liblist.a: list_adders.o list_removers.o list_movers.o
	ar -r -c -s liblist.a list_adders.o list_removers.o list_movers.o

list_adders.o: list_adders.c list.h
	${CC} ${CFLAGS} ${CPPFLAGS} -I. -c list_adders.c -o list_adders.o

list_removers.o: list_removers.c list.h
	${CC} ${CFLAGS} ${CPPFLAGS} -I. -c list_removers.c -o list_removers.o

list_movers.o: list_movers.c list.h
	${CC} ${CPPFLAGS} ${CFLAGS} -I. -c list_movers.c -o list_movers.o


#PPC
mycal-PPC: mycal-PPC.o
	${PPC_COMPILE} ${CFLAGS} -z noexecstack -o mycal-PPC mycal-PPC.o

basicServer-PPC: basicServer-PPC.o liblist-PPC.a
	${PPC_COMPILE} ${CFLAGS}  -z noexecstack -o basicServer-PPC basicServer-PPC.o -L./ -llist-PPC 

mycal-PPC.o: mycal.c
	${PPC_COMPILE} ${CFLAGS} -I. -c mycal.c -o mycal-PPC.o

basicServer-PPC.o: basicServerSelect.c server.h
	${PPC_COMPILE} ${CFLAGS} -I. -c basicServerSelect.c -o basicServer-PPC.o 

liblist-PPC.a: list_adders-PPC.o list_removers-PPC.o list_movers-PPC.o
	ar -r -c -s liblist-PPC.a \
		list_adders-PPC.o list_removers-PPC.o list_movers-PPC.o

list_adders-PPC.o: list_adders.c list.h
	${PPC_COMPILE} ${CFLAGS} ${CPPFLAGS} -I. -c list_adders.c \
		-o list_adders-PPC.o

list_removers-PPC.o: list_removers.c list.h
	${PPC_COMPILE} ${CFLAGS} ${CPPFLAGS} -I. -c list_removers.c \
		-o list_removers-PPC.o

list_movers-PPC.o: list_movers.c list.h
	${PPC_COMPILE} ${CPPFLAGS} ${CFLAGS} -I. -c list_movers.c \
		-o list_movers-PPC.o

#ARM

mycal-ARM: mycal-ARM.o
	${ARM_COMPILE} ${CFLAGS} -z noexecstack -o mycal-ARM mycal-ARM.o

basicServer-ARM: basicServer-ARM.o liblist-ARM.a
	${ARM_COMPILE} ${CFLAGS} -z noexecstack -o basicServer-ARM basicServer-ARM.o -L./ -llist-arm

mycal-ARM.o: mycal.c
	${ARM_COMPILE} ${CFLAGS} -I. -c mycal.c -o mycal-ARM.o

basicServer-ARM.o: basicServerSelect.c server.h
	${ARM_COMPILE} ${CFLAGS} -I. -c basicServerSelect.c -o basicServer-ARM.o

liblist-arm.a: list_adders-arm.o list_removers-arm.o list_movers-arm.o
	ar -r -c -s liblist-arm.a \
		list_adders-arm.o list_removers-arm.o list_movers-arm.o

list_adders-arm.o: list_adders.c list.h
	${ARM_COMPILE} ${CFLAGS} ${CPPFLAGS} -I. -c list_adders.c \
		-o list_adders-arm.o

list_removers-arm.o: list_removers.c list.h
	${ARM_COMPILE} ${CFLAGS} ${CPPFLAGS} -I. -c list_removers.c \
		-o list_removers-arm.o

list_movers-arm.o: list_movers.c list.h
	${ARM_COMPILE} ${CPPFLAGS} ${CFLAGS} -I. -c list_movers.c \
		-o list_movers-arm.o




clean:
	rm -f emulate server client *.o


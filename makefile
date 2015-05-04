CC=gcc
PORT=30305
CFLAGS = -DPORT=$(PORT) -g -Wall

all: battle

battle: battle.o
	${CC} ${CFLAGS} -o $@ battle.o

%.o: %.c
	${CC} ${CFLAGS} -c $<

clean:
	rm *.o battle

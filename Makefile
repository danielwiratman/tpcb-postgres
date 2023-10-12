CC = gcc
CFLAGS = -lpthread -lm -ldl -lrt -I /home/daniel/equnix-projects/eqpg-encrypt/postgresql-12.4/include -L /home/daniel/equnix-projects/eqpg-encrypt/postgresql-12.4/lib -lpq

all: test_postgres

test_postgres: test_postgres.o
	$(CC) -no-pie $(CFLAGS) -o test_postgres test_postgres.o

test_postgres.o: test_postgres.c
	$(CC) $(CFLAGS) -c test_postgres.c

clean:
	rm -f test_postgres test_postgres.o


CFLAGS = -std=gnu17 -g
LFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=reallocarray -Wl,--wrap=free -Wl,--wrap=strdup -Wl,--wrap=strndup

all: libma clean

test: testerki.c
	@gcc testerki.c -o test
	valgrind --leak-check=full -q --error-exitcode=1 ./test

libma: ma.o ma_example.o memory_tests.o
	@gcc $(LFLAGS) ma.o ma_example.o memory_tests.o -o libma
	valgrind --error-exitcode=123 --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all ./libma connection_stress
ma.o: ma.c
	@gcc $(CFLAGS) -c ma.c

ma_example.o: ma_example.c
	@gcc $(CFLAGS) -c ma_example.c

memory_tests.o: memory_tests.c
	@gcc $(CFLAGS) -c memory_tests.c

testerki.o : testerki.c
	@gcc -c testerki.c

clean:
	@echo "Cleaning..."
	rm *.o libma testerki

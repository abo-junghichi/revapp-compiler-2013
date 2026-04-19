CC=gcc -O3 -g -Wall -Wextra -pedantic -Wstrict-aliasing=1 -std=c89
FILE=viarevapp.c cpp.revapp debug.out a.out

all: debug.out a.out
mrproper:
	rm $(FILE) compile.out
clean:
	rm $(FILE)
compile.out: array.c compile.c readtoken.c compile-test.c
	$(CC) compile-test.c -o compile.out
cpp.revapp: test.revapp
	cpp -P -undef < test.revapp > cpp.revapp
viarevapp.c: compile.out cpp.revapp
	#./compile.out < cpp.revapp > viarevapp.c
	~/compile.out < cpp.revapp > viarevapp.c
debug.out: embed.c interp.c viarevapp.c
	$(CC) embed.c -o debug.out
a.out: embed.c interp.c viarevapp.c
	$(CC) embed.c -DNDEBUG -o a.out

#
#	Naman Jain
#	18CS10034
#

a.out: main.o disk.o sfs.o
	gcc  main.o disk.o sfs.o -lm
sfs.o: sfs.c sfs.h
	gcc -c -g sfs.c
disk.o: disk.c disk.h
	gcc -c -g disk.c
main.o: main.c disk.h sfs.h
	gcc -c -g main.c

clean:
	rm a.out sfs.o main.o disk.o
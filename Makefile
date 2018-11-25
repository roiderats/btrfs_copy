
all:
	cc -O3 -static -o btrfscopy btrfscopy.c
clean:
	[ -e ./btrfscopy ] && rm ./btrfscopy


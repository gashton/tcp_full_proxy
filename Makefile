all:
	gcc -g -o proxy proxy.c
	chmod +x proxy
clean:
	rm -f proxy

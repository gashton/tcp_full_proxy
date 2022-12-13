all:
	gcc -o tcp_full_proxy proxy.c
	chmod +x tcp_full_proxy
clean:
	rm -f tcp_full_proxy

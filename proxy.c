/*-
 * TCP Full Proxy
 *
 * Copyright 2015 Grant Ashton
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
#include "proxy.h"

void sig_handler(int signal)
{
	SHUTDOWN = 1;
}

int main(int argc, char *argv[])
{
	int error = 0;
	char* serverName;
	
	SHUTDOWN = 0;

	signal(SIGINT, sig_handler);

	//If invalid number of arguments are supplied, error.
	if(argc < 4 || argc > 5)
	{
		printf("TCP Full Proxy - (C) 2015 Grant Ashton\n");
		error = 1;
	}
	
	//Check if verbose mode is specified.
	if(strcmp(argv[1],"-v") == 0)
	{
		VERBOSE = 1;
		serverName = argv[2];
		localPort = (ushort) atoi(argv[3]) & 0xFFFF;
		remotePort = (ushort) atoi(argv[4]) & 0xFFFF;
	}
	else
	{
		VERBOSE = 0;
		serverName = argv[1];
		localPort = (ushort) atoi(argv[2]) & 0xFFFF;
		remotePort = (ushort) atoi(argv[3]) & 0xFFFF;
	}
	
     struct addrinfo hints;
	 memset(&hints, 0, sizeof(hints));
	 hints.ai_socktype = SOCK_STREAM;
	 hints.ai_family = AF_INET;
	
	//Resolve supplied hostname/ip to IPv4 address.
	if(getaddrinfo(serverName, NULL, &hints, &serverHost) != 0)
	{
		printf("Invalid IP or Hostname.\n");
		error = 1;
	}
	
	if(error)
	{
		printf("Usage: %s {-v} <ip of server> <local port> <remote port>\n",argv[0]);
		return 1;
	}

	listener();

	return 0;
}

void listener()
{
	int listenfd = 0, clientfd = 0, serverfd = 0, maxfd = 0, nbytes = 0, wbytes = 0, i = 0, j = 0, retval = 0, port = 0, yes = 1;
	struct sockaddr_in serv_addr;
	
	char recvBuff[1024];
	memset(&serv_addr, '0', sizeof(serv_addr));
	
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(localPort);

	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));

	listen(listenfd, MAX_CONNECTIONS);

	socklen_t len;
	struct sockaddr_storage addr;
	char ipstr[INET6_ADDRSTRLEN + 1];

	//Select() timeout.
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	//fd_set containing socket listener file descriptor
	//read_fds will be actively used and cleared when calling select()
	//master will be used for maintaining a set of socket file descriptors.
	fd_set master, read_fds, write_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_SET(listenfd, &master);

	//listenfd is highest fd to start with.
	maxfd = listenfd;
	
	//Connections array to store active connections.
	connection connections[MAX_CONNECTIONS];

	if(VERBOSE) printf("Initializing connections array\n");

	//Initialize connections array.
	for(i=0;i<MAX_CONNECTIONS;i++)
	{
		connections[i].clientfd = -1;
		connections[i].serverfd = -1;
	}

	printf("Proxy Started Listening\n");

	while(!SHUTDOWN)
	{
		read_fds = master;
		write_fds = master;

		//Blocks until a file descriptor is ready to be read or written, or timeout is reached.
		retval = select(maxfd+1, &read_fds, &write_fds, NULL, &timeout);

		if(retval == -1)
		{
			if(VERBOSE) printf("Listener: Error waiting for client/server i/o. (Interrupted?)\n");
			break;
		}
		else if(retval > 0)
		{
			//If we have a new connection.
			if(FD_ISSET(listenfd, &read_fds))
			{ 
				if(VERBOSE) printf("Listener: Attempting to accept new connection.\n");
				clientfd = accept(listenfd, (struct sockaddr*)&addr, &len);

				if(clientfd != -1)
				{
					//Get IP Address of peer.
					if (addr.ss_family == AF_INET) {
						struct sockaddr_in *s = (struct sockaddr_in *)&addr;
						port = ntohs(s->sin_port);
						inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
					} else { // AF_INET6
						struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
						port = ntohs(s->sin6_port);
						inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
					}

					if(VERBOSE) printf("Listener: Connection Accepted - %s \n", ipstr);

					//Try and connect to server.
					serverfd = server_connect(serverHost, remotePort);

					if(serverfd != -1)
					{
						//Update max file descriptor
						if(clientfd > maxfd)
						{
							maxfd = clientfd;
						}
						if(serverfd > maxfd)
						{
							maxfd = serverfd;
						}

						//Find free connections element and store file descriptors.
						for(j=0;j<MAX_CONNECTIONS;j++)
						{
							if(connections[j].clientfd == -1 || connections[j].serverfd == -1)
							{
								if(VERBOSE) printf("Listener: Adding ClientFD: %i and ServerFD: %i to master\n",clientfd,serverfd);
								connections[j].clientfd = clientfd;
								connections[j].serverfd = serverfd;

								//Add new connection fd to master set
								FD_SET(clientfd, &master);
								FD_SET(serverfd, &master);
								break;
							}
						}
					}
					else
					{
						shutdown(clientfd, 2);
						close(clientfd);
						if(VERBOSE) printf("Listener: Server connection failed, so client connection has been closed. \n");
					}
				}
				else
				{
					if(VERBOSE) printf("Listener: Error could not get client file descriptor - %s \n", ipstr);
				}
			}

			for(i = 0; i < MAX_CONNECTIONS; i++)
			{
				//Ensure file descriptors are defined for both server and client.
				if(connections[i].serverfd != -1 && connections[i].clientfd != -1)
				{
					//Received something from a server connection.
					if(FD_ISSET(connections[i].serverfd, &read_fds) && FD_ISSET(connections[i].clientfd, &write_fds))
					{ 
						if(VERBOSE) printf("Listener: Attempting to read server bytes for ClientFD: %i and ServerFD: %i\n",connections[i].clientfd,connections[i].serverfd);
						nbytes = read(connections[i].serverfd, recvBuff, sizeof(recvBuff));

						if(nbytes == -1)
						{
							if(VERBOSE) printf("Server Read error (Possibly reset connection?)\n");
						}
						else if(nbytes > 0)
						{
							wbytes = write(connections[i].clientfd, recvBuff, nbytes);
							if(VERBOSE) printf("S-C: Proxied %i bytes\n", nbytes);
						}
						else
						{
							if(VERBOSE) printf("Server closed connection.\n");
							wbytes = 0;
						}

						if(wbytes <= 0)
						{
							if(VERBOSE) printf("Client Read error (Possibly reset connection?)\n");
						}

						//Check for client or server errors.
						if(nbytes <= 0 || wbytes <= 0)
						{
							//Shutdown connections.
							shutdown(connections[i].serverfd, 2);
							close(connections[i].serverfd);
							shutdown(connections[i].clientfd, 2);
							close(connections[i].clientfd);

							FD_CLR(connections[i].clientfd, &master);
							FD_CLR(connections[i].serverfd, &master);

							connections[i].clientfd = -1;
							connections[i].serverfd = -1;
						}
					}

					//Received something from a client connection.
					if(FD_ISSET(connections[i].clientfd, &read_fds) && FD_ISSET(connections[i].serverfd, &write_fds))
					{
						if(VERBOSE) printf("Listener: Attempting to read client bytes for ClientFD: %i and ServerFD: %i\n",connections[i].clientfd,connections[i].serverfd);
						nbytes = read(connections[i].clientfd, recvBuff, sizeof(recvBuff));

						if(nbytes == -1)
						{
							if(VERBOSE) printf("Client Read error (Possibly reset connection?)\n");
						}
						else if(nbytes > 0)
						{
							wbytes = write(connections[i].serverfd, recvBuff, nbytes);
							if(VERBOSE) printf("C-S: Proxied %i bytes\n", nbytes);
						}
						else
						{
							if(VERBOSE) printf("Client closed connection.\n");
							wbytes = 0;
						}

						if(wbytes <= 0)
						{
							if(VERBOSE) printf("Server Read error (Possibly reset connection?)\n");
						}

						//Check for client or server errors.
						if(wbytes <= 0 || nbytes <= 0)
						{
							//Shutdown connections.
							shutdown(connections[i].clientfd, 2);
							close(connections[i].clientfd);
							shutdown(connections[i].serverfd, 2);
							close(connections[i].serverfd);

							FD_CLR(connections[i].clientfd, &master);
							FD_CLR(connections[i].serverfd, &master);

							connections[i].clientfd = -1;
							connections[i].serverfd = -1;
						}
					}
				}
			}
		}
	}

	//Close listener.
	shutdown(listenfd, 2);
	close(listenfd);

	//Close all connections.
	for(i=0;i<MAX_CONNECTIONS;i++)
	{
		if(connections[i].clientfd != -1)
		{
			if(VERBOSE) printf("Listener: Closing ClientFD: %i connection\n",connections[i].clientfd);
			shutdown(connections[i].clientfd, 2);
			close(connections[i].clientfd);
		}

		if(connections[i].serverfd != -1)
		{
			if(VERBOSE) printf("Listener: Closing ServerFD: %i connection\n",connections[i].serverfd);
			shutdown(connections[i].serverfd, 2);
			close(connections[i].serverfd);
		}
	}
	
	freeaddrinfo(serverHost);

	printf("Proxy Stopped Listening\nGoodbye\n");
}

//Create a connection to the server
int server_connect(struct addrinfo *host, int port)
{
	int serverfd;
	struct sockaddr_in serv_addr; 

	memset(&serv_addr, '0', sizeof(serv_addr));

	if((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Error: Could not create socket to server \n");
		return -1;
	}

	serv_addr.sin_addr = ((struct sockaddr_in *) host->ai_addr)->sin_addr; 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port); 

	//Non-Blocking
	fcntl(serverfd, F_SETFL, O_NONBLOCK);

	if(VERBOSE) printf("Connection to server has been attempted.\n");

	connect(serverfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	return serverfd;
}
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
	SHUTDOWN = 0;
	char* serverName;
	int i, localPortSupplied = 0, remotePortSupplied = 0, hostSupplied = 0;

	signal(SIGINT, sig_handler);

	for(i=1;i<argc;i++)
	{
		if(strcmp(argv[i],"-v") == 0)
		{
			VERBOSE = 1;
		}
		else if(strcmp(argv[i],"-l") == 0)
		{
			localPortSupplied = 1;
			localPort = (ushort) atoi(argv[i+1]) & 0xFFFF;
		}
		else if(strcmp(argv[i],"-h") == 0)
		{
			hostSupplied = 1;
			serverName = argv[i+1];
		}
		else if(strcmp(argv[i],"-p") == 0)
		{
			remotePortSupplied = 1;
			remotePort = (ushort) atoi(argv[i+1]) & 0xFFFF;
		}
	}

	if(!localPortSupplied || !remotePortSupplied || !hostSupplied)
	{
		printf("TCP Full Proxy - (C) 2015 Grant Ashton\n");

		if(!localPortSupplied)
		{
			printf("Please specify the local port to listen on.\n");
		}
		if(!hostSupplied)
		{
			printf("Please specify the host to proxy to.\n");
		}
		if(!remotePortSupplied)
		{
			printf("Please specify the remote port to proxy to.\n");
		}

		usage(argv[0]);
		return 1;
	}

	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;

	//Resolve supplied hostname/ip to IPv4 address.
	if(getaddrinfo(serverName, NULL, &hints, &serverHost) != 0)
	{
		printf("Invalid IP or Hostname.\n");
		usage(argv[0]);
		return 1;
	}

	//Start listener.
	listener();

	return 0;
}

void listener()
{
	int listenfd = 0, clientfd = 0, serverfd = 0, maxfd = 0, i = 0, j = 0, nrfd = 0, nwfd = 0, yes = 1;
	struct sockaddr_in serv_addr;

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
		//Reset fds.
		read_fds = master;
		write_fds = master;

		//Reset timeout.
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		//Blocks until a file descriptor is ready to be read or timeout is reached.
		nrfd = select(maxfd+1, &read_fds, NULL, NULL, &timeout);

		if(nrfd < 0)
		{
			if(VERBOSE) printf("Listener: Error waiting for client/server i/o. (Interrupted?)\n");
			break;
		}
		else if(nrfd > 0)
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
						inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
					} else { // AF_INET6
						struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
						inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
					}

					if(VERBOSE) printf("Listener: Connection Accepted - %s \n", ipstr);

					//Try and connect to server.
					serverfd = server_connect(serverHost, remotePort);

					if(serverfd != -1)
					{
						//Find free connections element and store file descriptors.
						for(j=0;j<MAX_CONNECTIONS;j++)
						{
							if(connections[j].clientfd == -1 && connections[j].serverfd == -1)
							{
								if(VERBOSE) printf("Listener: Adding ClientFD: %i and ServerFD: %i to master\n",clientfd,serverfd);
								connections[j].clientfd = clientfd;
								connections[j].serverfd = serverfd;
								
								//Update max file descriptor.
								if(clientfd > maxfd)
								{
									maxfd = clientfd;
								}
								if(serverfd > maxfd)
								{
									maxfd = serverfd;
								}

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

			for(i = 0; i < nrfd; i++)
			{
				//Ensure file descriptors are defined for both server and client.
				if(connections[i].serverfd != -1 && connections[i].clientfd != -1)
				{
					//Reset timeout.
					timeout.tv_sec = 1;
					timeout.tv_usec = 0;

					//Check there are file descriptors ready to be written to.
					nwfd = select(maxfd+1, NULL, &write_fds, NULL, &timeout);

					if(nwfd > 0)
					{
						//Check if serverfd is ready to be written to and clientfd is ready to be ready from.
						if(FD_ISSET(connections[i].serverfd, &write_fds) && FD_ISSET(connections[i].clientfd, &read_fds))
						{ 
							//Client to Server
							proxy_data(&connections[i], &master, 0);
						}

						//Check if clientfd is ready to be written to and serverfd is ready to be ready from.
						if(FD_ISSET(connections[i].clientfd, &write_fds) && FD_ISSET(connections[i].serverfd, &read_fds))
						{ 
							//Server to Client
							proxy_data(&connections[i], &master, 1);
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

void proxy_data(connection *con, fd_set* master, int isServerToClient)
{
	int sourcefd, destfd, nbytes, wbytes;
	char recvBuff[65536];

	//Server to Client
	if(isServerToClient)
	{
		sourcefd = con->serverfd;
		destfd = con->clientfd;
	}
	//Client to Server
	else
	{
		sourcefd = con->clientfd;
		destfd = con->serverfd;
	}

	if(VERBOSE) printf("Attempting to read bytes (Sourcefd:%i, Destfd: %i)\n", sourcefd, destfd);
	nbytes = read(sourcefd, recvBuff, sizeof(recvBuff));

	if(nbytes == -1)
	{
		if(isServerToClient)
		{
			if(VERBOSE) printf("Server Read error (Possibly reset connection?)\n");
		}
		else
		{
			if(VERBOSE) printf("Client Read error (Possibly reset connection?)\n");
		}
	}
	else if(nbytes > 0)
	{
		wbytes = write(destfd, recvBuff, nbytes);

		if(isServerToClient)
		{
			if(VERBOSE) printf("S-C: Proxied %i bytes\n", nbytes);
		}
		else
		{
			if(VERBOSE) printf("C-S: Proxied %i bytes\n", nbytes);
		}

	}
	else
	{
		if(isServerToClient)
		{
			if(VERBOSE) printf("Server closed connection.\n");
		}
		else
		{
			if(VERBOSE) printf("Client closed connection.\n");
		}
		
		wbytes = 0;
	}

	//If failed to write any bytes, and there were bytes read that needed writing.
	if(wbytes <= 0 && nbytes > 0)
	{
		if(isServerToClient)
		{
			if(VERBOSE) printf("Client Write error (Possibly reset connection?)\n");
		}
		else
		{
			if(VERBOSE) printf("Server Write error (Possibly reset connection?)\n");
		}
	}

	//Check for client or server errors.
	if(nbytes <= 0 || wbytes <= 0)
	{
		if(VERBOSE) printf("Closing connections (Sourcefd:%i, Destfd: %i)\n", sourcefd, destfd);

		//Shutdown connections.
		shutdown(sourcefd, 2);
		close(sourcefd);
		shutdown(destfd, 2);
		close(destfd);

		FD_CLR(sourcefd, master);
		FD_CLR(destfd, master);

		con->clientfd = -1;
		con->serverfd = -1;
	}
}

void usage(char* programName)
{
	printf("Usage: %s {-v} -l <local port> -h <host> -p <remote port>\n", programName);
}
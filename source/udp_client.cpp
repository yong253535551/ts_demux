#include <stdio.h>   
#include <string.h>   
#include <errno.h>   
#include <stdlib.h>   
#include <unistd.h>
#include <fcntl.h> 
#include <sys/types.h>   
#include <sys/socket.h>   
#include <netinet/in.h>   
#include <arpa/inet.h>

#include <pthread.h>

#include "udp_client.h"

UDPClient::UDPClient()
{
	m_thread_id_udp = -1;	
}

UDPClient::~UDPClient()
{

}

int UDPClient::Open(int port)
{
	int clientSocket = -1;
	int optVal = 1;
    struct sockaddr_in ser_addr;

    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if(clientSocket < 0){
        printf("create socket fail!\n");
        return -1;
    }

	if (setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optVal, sizeof(optVal)) < 0) {
    	printf("setsockopt(SO_REUSEADDR) error: \n");
    	close(clientSocket);
    	return -1;
  	}

    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ser_addr.sin_port = htons(port);

	if(bind(clientSocket, (struct sockaddr*)&ser_addr, sizeof(ser_addr))!=0){
  		printf("Can't bind socket to local port!Program stop.\n");
		close(clientSocket);
  		return -1;
 	}

	int curFlags = fcntl(clientSocket, F_GETFL, 0);
  	fcntl(clientSocket, F_SETFL, curFlags|O_NONBLOCK);

	return clientSocket;
}

void UDPClient::Close(int fd)
{
	close(fd);
}

int UDPClient::StartReceiveThread(void *(*handle) (void *), void *arg)
{
    if(pthread_create(&m_thread_id_udp, NULL, handle, (void *)arg) != 0){
        printf("fail to pthread_create...\n");
		return 01;
    }else{
        printf("success to pthread_create...\n");
    }

	return 0;
}

void UDPClient::StopReceiveThread(void)
{
	pthread_join(m_thread_id_udp, 0);
}

int UDPClient::ReadSocket(int socket, unsigned char* buffer, unsigned bufferSize,
           struct sockaddr_in& fromAddress)
{
	socklen_t addressSize = sizeof(fromAddress);
  	int bytesRead = recvfrom(socket, (char*)buffer, bufferSize, 0, (struct sockaddr*)&fromAddress, &addressSize);
  	if (bytesRead < 0) {
    	printf("recvfrom() error: \n");
  	} else if (bytesRead == 0) {
		printf("recvfrom() bytesRead == 0 \n");
    	// "recvfrom()" on a stream socket can return 0 if the remote end has closed the connection.  Treat this as an error:
    	return -1;
  	}

	return bytesRead;
}

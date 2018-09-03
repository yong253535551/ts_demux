#ifndef __UDP_CLIENT_H__
#define __UDP_CLIENT_H__

#define UDP_SERVER_PORT 24080
#define MTU_SIZE 1500

struct rtp_header {
    uint8_t version;
    uint8_t pt;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t ssrc;
};

class UDPClient
{
public:
	UDPClient();
	~UDPClient();

	int Open(int port);
	void Close(int fd);

	int StartReceiveThread(void *(*handle) (void *), void *arg);
	void StopReceiveThread(void);

	int ReadSocket(int socket, unsigned char* buffer, unsigned bufferSize,
           struct sockaddr_in& fromAddress);
private:
	pthread_t m_thread_id_udp;
};

#endif

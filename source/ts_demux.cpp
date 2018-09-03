#include "mpeg-ps.h"
#include "mpeg-ts.h"
#include "mpeg-ts-proto.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include <pthread.h>

#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "udp_client.h"
#include "circular_buffer.h"

#define TS_PACKET_SIZE 188
#define DEFAULT_TS_PACKETS_IN_ITEM      7

#define VIDEO_ES_FILE "yong.h264"
#define AUDIO_ES_FILE "yong.aac"

typedef struct{
	FILE* vfp;
	FILE* afp;
	
	int sock;
	pthread_t thread_id_udp;
}ts_demux;

ts_demux g_ts_demux;

inline const char* ftimestamp(int64_t t, char* buf)
{
	if (PTS_NO_VALUE == t)
	{
		sprintf(buf, "(null)");
	}
	else
	{
		t /= 90;
		sprintf(buf, "%d:%02d:%02d.%03d", (int)(t / 36000000), (int)((t / 60000) % 60), (int)((t / 1000) % 60), (int)(t % 1000));
	}
	return buf;
}

static void on_ts_packet(void* /*param*/, int /*stream*/, int avtype, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
	static char s_pts[64], s_dts[64];

	if (PSI_STREAM_AAC == avtype)
	{
		static int64_t a_pts = 0, a_dts = 0;
        if (PTS_NO_VALUE == dts)
            dts = pts;
		//assert(0 == a_dts || dts >= a_dts);
		printf("[A] pts: %s(%ld), dts: %s(%ld), diff: %03d/%03d\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - a_pts) / 90, (int)(dts - a_dts) / 90);
		a_pts = pts;
		a_dts = dts;

		fwrite(data, 1, bytes, g_ts_demux.afp);
	}
	else if (PSI_STREAM_H264 == avtype)
	{
		static int64_t v_pts = 0, v_dts = 0;
		//assert(0 == v_dts || dts >= v_dts);
		printf("[V] pts: %s(%ld), dts: %s(%ld), diff: %03d/%03d%s\n", ftimestamp(pts, s_pts), pts, ftimestamp(dts, s_dts), dts, (int)(pts - v_pts) / 90, (int)(dts - v_dts) / 90, flags ? " [I]":"");
		v_pts = pts;
		v_dts = dts;

		fwrite(data, 1, bytes, g_ts_demux.vfp);
	}
	else
	{
		//assert(0);
	}
}

void mpeg_ts_dec_test(const char* file)
{
	unsigned char ptr[TS_PACKET_SIZE];
	FILE* fp = fopen(file, "rb");
	g_ts_demux.vfp = fopen(VIDEO_ES_FILE, "wb");
	g_ts_demux.afp = fopen(VIDEO_ES_FILE, "wb");
    
    ts_demuxer_t *ts = ts_demuxer_create(on_ts_packet, NULL);
	if(!ts){
		printf("ts is NULL...\n");
		return;
	}

	while (1 == fread(ptr, sizeof(ptr), 1, fp))
	{
        ts_demuxer_input(ts, ptr, sizeof(ptr));
	}

    ts_demuxer_flush(ts);
    ts_demuxer_destroy(ts);

	fclose(fp);
	fclose(g_ts_demux.vfp);
	fclose(g_ts_demux.afp);
}

static void *process_udp_data(void *param)
{
    struct sockaddr_in src;
    socklen_t len = sizeof(struct sockaddr);
    char rbuf[MTU_SIZE];
    int bytesRead;
    int bytesWrite;

    struct timeval tv;
    fd_set readfds;
    int ret = -1;
    int fd = g_ts_demux.sock;

    tv.tv_sec = 6;
    tv.tv_usec = 0;

	g_ts_demux = *(ts_demux *)param;
	printf("g_ts_demux.sock = %d fd = %d...\n", g_ts_demux.sock, fd);

	ts_demuxer_t *ts = ts_demuxer_create(on_ts_packet, NULL);
	if(!ts){
		printf("ts is NULL...\n");
		return NULL;
	}

    while(1){
		FD_ZERO(&readfds);
		FD_SET(0, &readfds);
		FD_SET(fd, &readfds);
		memset(rbuf, 0, MTU_SIZE);
		ret = select(fd+1, &readfds, NULL, NULL, &tv);
		if(ret < 0){
	    	printf("select error...\n");
		}else if(ret == 0){
	    	//printf("select timeout...\n");
	    	continue;
		}else{
	    	if(FD_ISSET(fd, &readfds)){
	    		bytesRead = recvfrom(fd, rbuf, MTU_SIZE, 0, (struct sockaddr*)&src, &len);
	    		if(bytesRead < 0){
		    		printf("recvfrom() error: \n");
	    		}else if(bytesRead == 0){
		    		printf("recvfrom() bytesRead == 0 \n");
	    		}

	    		bytesWrite = bytesRead - sizeof(struct rtp_header);				

				byte *tempbuf = (byte *)rbuf + sizeof(struct rtp_header);
				
				int num = bytesWrite / TS_PACKET_SIZE;
				for(int i = 0; i < num; i++){
					if(tempbuf[i * TS_PACKET_SIZE] != 0x47){
						printf("data[0] != 0x47 i = %d num = %d\n", i, num);
					}
					ts_demuxer_input(ts, tempbuf + i * TS_PACKET_SIZE, TS_PACKET_SIZE);
				}
	    	}
		}	
    }

	ts_demuxer_flush(ts);
	ts_demuxer_destroy(ts);

	fclose(g_ts_demux.vfp);
	fclose(g_ts_demux.afp);

    return 0;
}

int start_receive_thread(void)
{
	if(pthread_create(&g_ts_demux.thread_id_udp, NULL, process_udp_data, (void *)&g_ts_demux) != 0){
		printf("fail to pthread_create...\n");
		return -1;
    }else{
		printf("success to pthread_create...\n");
    }

	return 0;
}

int main(int argc, char **argv)
{
	g_ts_demux.vfp = NULL;
	g_ts_demux.afp = NULL;
	g_ts_demux.sock = 0;
	g_ts_demux.thread_id_udp = -1;

	g_ts_demux.vfp = fopen(VIDEO_ES_FILE, "wb");
	if(!g_ts_demux.vfp){
		printf("fail to open %s...\n", VIDEO_ES_FILE);
		return -1;
	}
	g_ts_demux.afp = fopen(AUDIO_ES_FILE, "wb");
	if(!g_ts_demux.afp){
		printf("fail to open %s...\n", AUDIO_ES_FILE);
		return -1;
	}

	UDPClient *udpClient = new UDPClient();
	g_ts_demux.sock = udpClient->Open(UDP_SERVER_PORT);
	if(g_ts_demux.sock < 0){
		printf("fail to open udp socket...\n");
		return -1;
	}
	
	if(start_receive_thread() < 0){
		printf("fail to start_receive_thread...\n");
		return -1;
	}

	while(1){
		sleep(1);
	}
	
	return 0;
}

#include "unp.h"
#include "unpifiplus.h"
//#include "common_header.h"
#include "unprtt_plus.h"
#include "setjmp.h"
#include "buffer_queue.h"
#include "pthread.h"

#define NINTERFACES 10
#define BUFFLEN 20
static sigjmp_buf jmpBuf;
void *producer_thread(void *arg);
void *consumer_thread(void *arg);
pthread_mutex_t mutex;
struct conn_data{
	int sockfd;
	struct cirQueue *pQbuff;
	int msg_flag;
	struct packet_data packet;
	int seq;
};
int main(int argc, char** argv)
{
    int sockfd;
    char fileName[100];
	struct cirQueue recvBuff;
    struct sockaddr_in servinfo;
	struct sockaddr_in cliinfo;
	struct ifi_info *ifi, *ifihead;
	struct sock_ifinfo soif_map[NINTERFACES];
	int msg_flag=0;
	int total_ifs=0;
	int inf;
	int i;
	int recvd;
	struct rtt_info rttInfo;
	int sockFlag;
	struct packet_data sendData;
	struct packet_data recvData;
	int newPort;
	struct conn_data connDetails;
	int seq=10000;
	ifihead = Get_ifi_info_plus(AF_INET, 1);
	sockfd = socket(AF_INET, SOCK_DGRAM,0);
	if( (sockFlag = fcntl(sockfd, F_GETFL, 0)) > 0)
	{
		// set the connectfd to nonblocking
		sockFlag |= O_NONBLOCK;
		if(fcntl(sockfd,F_SETFL,sockFlag) < 0)
			printf(" unable to set connect fd as nonblocking in child\n");
	}
	bzero(&cliinfo, sizeof(struct sockaddr_in));
	
	for(i=0;i<NINTERFACES;i++)
	{
		bzero(&soif_map[i], sizeof(struct sock_ifinfo) );
	}
	
	for(ifi = ifihead,i=0; ifi!=NULL; ifi=ifi->ifi_next,i++)
	{
		struct sockaddr *sa;
		sa = ifi->ifi_addr;
		strcpy(soif_map[i].ip_addr,Sock_ntop_host(sa, sizeof(*sa)));
		sa = ifi->ifi_ntmaddr;
		strcpy(soif_map[i].net_mask,Sock_ntop_host(sa,sizeof(*sa)));
		get_subaddr(soif_map[i].ip_addr, soif_map[i].net_mask,soif_map[i].subnet_addr);
	}
	cliinfo.sin_family = AF_INET;
	if(isAddress_local(soif_map,total_ifs,argv[1],&inf))
	{
		inet_pton(AF_INET, soif_map[inf].ip_addr, &(cliinfo.sin_addr));
		msg_flag = MSG_DONTROUTE;
	}
	else
		cliinfo.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(sockfd, (struct sockaddr*)&cliinfo, sizeof(struct sockaddr_in));
    bzero(&servinfo,sizeof(struct sockaddr_in));
    servinfo.sin_family = AF_INET;
    servinfo.sin_port = htons(5577);
    if(inet_pton(AF_INET, argv[1],&(servinfo.sin_addr)) !=1)
    {
        printf(" err: %s",strerror(errno));
        exit(-1);
    }
    printf("coming here\n");
	// check the ip is local
    if(connect(sockfd, (struct sockaddr *)&servinfo, sizeof(struct sockaddr_in))!=0)
    {
	    printf("error: %s", strerror(errno));
    }
	scanf("%s\n",fileName);

	sendData.seqNum = seq++;
	sendData.ackNum = 0;
	sendData.msgType = SYNC;
	sendData.sendts = rtt_ts(&rttInfo);
	sendData.recvts = 0;
	sendData.wSize = BUFFLEN;
	memcpy(sendData.udpdata, fileName, strlen(fileName));
	rtt_newpack(&rttInfo);
	alarm(rtt_start(&rttInfo));
	send(sockfd, &sendData, sizeof(struct packet_data), msg_flag);
	if(sigsetjmp(jmpBuf, 0) !=0)
	{
		if(rtt_timeout(&rttInfo) <0)
		{
			printf(" connection timed out ");
			exit(-1);
		}
		else
		{
			// resend the packet 
			alarm(rtt_start(&rttInfo));
			send(sockfd, &sendData, sizeof(struct packet_data), msg_flag);
			sendData.time_stamp = rtt_ts(&rttInfo);
		}
	}
	do{
		recvd =  recv(sockfd, &recvData, sizeof(struct packet_data),0);
	}while(sendData.seqNum != recvData.ackNum -1);
	alarm(0);
	rtt_stop(&rttInfo, rtt_ts(&rttInfo) - recvData.recvts);
	// connect on to server's new port
	
	newPort = atoi(recvData.udpdata);
	servinfo.sin_port = newPort;
	connect(sockfd, (struct sockaddr*)&servinfo, sizeof(struct sockaddr_in));
	sendData.msgType = ACK;
	sendData.sendts = rtt_ts(&rttInfo);
	sendData.seqNum = seq++;
	sendData.ackNum = recvData.seqNum+1;
	send(sockfd, &sendData, sizeof(struct packet_data), msg_flag);
	// if server received this connection it starts sending data, otherwise, server will send ACKSYNC message
	do{
		recvd = recv(sockfd, &recvData, sizeof(struct packet_data), 0);
		if(recvData.msgType == ACKSYNC)
		{
			// resend the ACK to the child server
			sendData.sendts = rtt_ts(&rttInfo);
			sendData.recvts = recvData.sendts;
			send(sockfd, &sendData, sizeof(struct packet_data), msg_flag);
		}
	}while(recvData.msgType != DATA);
	queue_add(&recvBuff, &recvData);
	// data to be passed to producer and consumer threads
	connDetails.msg_flag = msg_flag;
	connDetails.pQbuff = &recvBuff;
	connDetails.sockfd = sockfd;
	connDetails.packet = recvData;
	connDetails.seq = seq;
	// start producer and consumer thread
	pthread_t prodt_id;
	pthread_t const_id;
	pthread_create(&prodt_id, NULL, producer_thread, &connDetails);
	pthread_create(&const_id, NULL, consumer_thread, &connDetails);
	pthread_join(prodt_id, NULL);
	pthread_join(const_id, NULL);

}
void *producer_thread(void *arg)
{
	struct conn_data conn = *(struct conn_data *)arg;
	int sockfd = conn.sockfd;
	struct cirQueue *pBuffQ = conn.pQbuff;
	int msg_flag = conn.msg_flag;
	struct packet_data sendData;
	struct packet_data recvData =  conn.packet;
	int seq = conn.seq;
	int next_exp = recvData.seqNum +1;
	int recvd;
	// keep receiving messages until msgType = FIN
	do{
		sendData.sendts = 0;// i don't need to calculate rtt
		sendData.recvts = recvData.sendts;
		sendData.ackNum = next_exp;
		sendData.msgType = ACK;
		sendData.wSize = queue_availBuff(pBuffQ);
		sendData.seqNum = seq++;
		memset(sendData.udpdata, 0, sizeof(sendData.udpdata));
		send(sockfd, &sendData, sizeof(struct packet_data),msg_flag);
		do{
			recvd = recv(sockfd, &recvData, sizeof(struct packet_data), 0);
			if( recvData.seqNum != next_exp)
			{
				// reask the server to send next segment
				sendData.sendts = 0;
				sendData.recvts = recvData.sendts;
				sendData.ackNum = next_exp;
				send(sockfd, &sendData, sizeof(struct packet_data), 0);
			}
			else
			{
				// add this to buffer for consumer thread to read
				pthread_mutex_lock(&mutex);
				queue_add(pBuffQ, &recvData);
				pthread_mutex_unlock(&mutex);
				
			}
		}while(recvData.seqNum ==  next_exp);
		next_exp++;
	}while(recvData.msgType != FIN);
	// connection closed, send FIN_ACK and exit
	sendData.msgType = FINACK;
	send(sockfd, &sockfd, sizeof(struct packet_data), msg_flag);
	return NULL;
}
void *consumer_thread(void *arg)
{
	struct conn_data  conn = *(struct conn_data *)arg;
	struct cirQueue *pBuffQ  = conn.pQbuff;
	struct packet_data temp;
	memset(&temp,0, sizeof(struct packet_data) );
	do{
		pthread_mutex_lock(&mutex);
		while(queue_read(pBuffQ, &temp)>0) 
		{
			// print the data
			printf("%s",temp.udpdata);
		}
		pthread_mutex_unlock(&mutex);
		sleep(5);
	}while(temp.msgType != FIN);
	
	return NULL;
}
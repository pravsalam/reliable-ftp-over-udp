#include<stdio.h>
#include "unpifiplus.h"
#define BUFSIZE 20
int get_subaddr(char *, char *, char *);
int isAddress_local(struct sock_ifinfo [], int , char* , int* );
int find_maxfd(struct sock_ifinfo [], int );
typedef enum {SYNC, ACK, ACKSYNC, DATA, FIN, FINACK} packetType;
/*struct sock_ifinfo{
	int sockfd;
	char ip_addr[IP_LENGTH];
	char net_mask[NMASK_LENGTH];
	char subnet_addr[SUBNET_LENGTH];
};*/
struct packet_data{
	time_t time_stamp;
	time_t sendts;
	time_t recvts;
	int seqNum;
	int ackNum;
	int hLength;
	int wSize;
	int msgType;
	char udpdata[512];
};
struct buffer{
	int time_stamp;
	int seqNumber;
	int ackNumber;
	char *data;
};


#include "unp.h"
#include "unpifiplus.h"
//#include "common_header.h"
#include "buffer_queue.h"
#include "unprtt_plus.h"
#include "setjmp.h"
#include "file_queue.h"
#define NINTERFACES 10
void sig_alarm(int signo);
static sigjmp_buf jmpbuf; 
int main()
{
	struct sockaddr_in servinfo;
	struct sockaddr_in cliinfo;
	struct sockaddr *sa;
	int listenfd;
	int connectfd;
	int recvd = 0;
	char buff[1024];
	struct packet_data udpPacket;
	socklen_t sockaddrinlen;
	char fileName[100];
	int soReuseaddr=1;
	int i;
	struct ifi_info *ifi, *ifihead;
	struct sock_ifinfo soif_map[NINTERFACES];
	int total_ifs;
	fd_set descset;
	int cliWndwSize,cliSeqNum; 
	packetType cliMsgType; 
	ifihead = Get_ifi_info_plus(AF_INET, 1);
	for(i=0;i<NINTERFACES;i++)
	{
		bzero(&soif_map[i], sizeof(struct sock_ifinfo) );
	}
	for(ifi = ifihead,i=0; ifi!=NULL; ifi=ifi->ifi_next,i++)
	{
		if((listenfd = socket(AF_INET, SOCK_DGRAM, 0))<0)
		{
			printf("Error: %s",strerror(errno));
			exit(-1);
		}
		soif_map[i].sockfd = listenfd;
		sa = ifi->ifi_addr;
		strcpy(soif_map[i].ip_addr,Sock_ntop_host(sa, sizeof(*sa)));
		printf(" ip = %s\n",soif_map[i].ip_addr);
		sa = ifi->ifi_ntmaddr;
		strcpy(soif_map[i].net_mask,Sock_ntop_host(sa,sizeof(*sa)));
		get_subaddr(soif_map[i].ip_addr, soif_map[i].net_mask,soif_map[i].subnet_addr);
		servinfo.sin_family = AF_INET;
		servinfo.sin_addr.s_addr = htons(INADDR_ANY);
		servinfo.sin_port = htons(5577);
		sockaddrinlen = sizeof(struct sockaddr_in);
		if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &soReuseaddr,sizeof(soReuseaddr)) == -1)
		{
			printf("unable to set socket option SO_REUSEADDR: %s\n",strerror(errno));
		}
		if(bind(listenfd,(struct sockaddr*)&servinfo, sockaddrinlen)== -1)
		{
			printf("bind error: %s",strerror(errno));
		}
	}
	total_ifs = i;
	int maxfd = find_maxfd(soif_map,total_ifs);
	FD_ZERO(&descset);
	while(1)
	{
		for(i=0;i<total_ifs;i++)
		{
			FD_SET(soif_map[i].sockfd,&descset);
		}
		if(select(maxfd+1, &descset, NULL, NULL, NULL) == -1)
		{
			if( errno == EINTR)
				continue;
			else
				exit(-1);
		}
		for(i= 0;i<total_ifs; i++)
		{
			if(FD_ISSET(soif_map[i].sockfd,&descset))
			{
				recvd = recvfrom(listenfd,buff,sizeof(buff),0,(struct sockaddr*)&cliinfo,&sockaddrinlen );
				udpPacket = *((struct packet_data*)buff);
				cliWndwSize = udpPacket.wSize;
				cliSeqNum = udpPacket.seqNum;
				cliMsgType = udpPacket.msgType;
				// buff has window Size, msg type, etc info for use later
				if(cliMsgType == SYNC && recvd >0)
				{
					// fork a new process to handle the connection 
					if(fork() ==0)
					{
						int j;
						int locif_num;
						struct sockaddr_in chldservInfo;
						struct sockaddr_in infoSelf;
						int bindPort;
						int cwindow;
						int ssthreshold;
						int msg_flag;
						struct cirQueue sendBuff;
						struct cirQueue recvBuff;
						struct fileSegQueue fileQ;
						struct rtt_info rttInfo;
						FILE *fp;
						int connSockFlag;
						int seq;
						
						// initialize 
						Signal(SIGALRM, sig_alarm);
						queue_init(&recvBuff);
						queue_init(&sendBuff);
						fileq_init(&fileQ);
						rtt_init(&rttInfo);
						seq = 1000;
						//child process, close other sockets here except the listening socket and 
						for(j =0;j <total_ifs;j++)
						{
							if (soif_map[j].sockfd != soif_map[i].sockfd)
							{
								// close the socket 
								close(soif_map[j].sockfd);
							}
						}
						// create a new socket
						connectfd = socket(AF_INET, SOCK_DGRAM, 0);
						chldservInfo.sin_family = AF_INET;
						chldservInfo.sin_port = htons(0);
						// check if the client IP is local subnet
						if(isAddress_local(soif_map,total_ifs,Sock_ntop((struct sockaddr*)&cliinfo, sizeof(struct sockaddr)),&locif_num))
						{
							// if it is local, send the reply on local if
							printf("client is local");
							msg_flag = MSG_DONTROUTE;
							inet_pton(AF_INET, soif_map[locif_num].ip_addr, &(chldservInfo.sin_addr));
						}
						else
						{
							// It is not local so choose any 
							printf(" Client is not local\n");
							msg_flag = 0;
							chldservInfo.sin_addr.s_addr = htonl(INADDR_ANY);
							
						}
						//bind the child 
						if (bind(connectfd, (struct sockaddr *)&chldservInfo, sizeof(struct sockaddr_in)) <0)
						{
							printf(" failed to bind in child process:%s\n",strerror(errno));
							exit(-1);
						}
						// ask kernal for port number
						if(getsockname(connectfd, (struct sockaddr *)&infoSelf, &sockaddrinlen) <0)
						{
							printf("unable to get the child bind port number: %s",strerror(errno));
							exit(-1);
						}
						else
						{
							char bindIp[50];
							int bindPort;
							inet_ntop(AF_INET, &(infoSelf.sin_addr),bindIp, sockaddrinlen);
							bindPort = ntohs(infoSelf.sin_port);
						}
						// connect to client
						connect(connectfd, (struct sockaddr*)&cliinfo, sockaddrinlen);
						rtt_newpack(&rttInfo);
						alarm(rtt_start(&rttInfo));
						
						// send and place the acksync on send buffer
						struct packet_data sendData; 
						struct packet_data recvData;
						sendData.ackNum = cliSeqNum +1;
						sendData.seqNum = seq; // get it from random with seed
						sendData.time_stamp = rtt_ts(&rttInfo);
						sendData.wSize = queue_availBuff(&recvBuff);
						sscanf(sendData.udpdata,"%d",&bindPort);
						send(listenfd, &sendData, sizeof(struct packet_data),msg_flag);
						send(connectfd, &sendData, sizeof(struct packet_data),msg_flag);
						if(sigsetjmp(jmpbuf, 0)!=0)
						{
							if(rtt_timeout(&rttInfo)<0)
							{
								// max retryCount has reached
								printf("client not responding\n");
								exit(-1);
							}
							else
							{
								//rupdate time stamp and resent packet
								sendData.time_stamp = rtt_ts(&rttInfo);
								send(listenfd, &sendData, sizeof(struct packet_data),msg_flag);
								send(connectfd, &sendData, sizeof(struct packet_data),msg_flag);
							}
						}
						do{
							recv(connectfd, &recvData, sizeof(struct packet_data),0);
						}while(recvData.ackNum != sendData.seqNum+1);
						alarm(0);
						rtt_stop(&rttInfo, rtt_ts(&rttInfo) - recvData.time_stamp);
						
						//close the listen descriptor
						close(listenfd);
						// start the file Transfer
						fp = fopen(fileName, "r");
						cwindow  = 1; 
						ssthreshold = cliWndwSize;
						if( (connSockFlag = fcntl(connectfd, F_GETFL, 0)) > 0)
						{
							// set the connectfd to nonblocking
							connSockFlag |= O_NONBLOCK;
							if(fcntl(connectfd,F_SETFL,connSockFlag) < 0)
								printf(" unable to set connect fd as nonblocking in child\n");
						}
						// start of file transfer
						do{
							while(fileQ.size < cwindow )
							{
								seq++;
								// previously read content is 
								fread(buff, sizeof(char), 512,fp);
								fileq_add(&fileQ,seq, buff);
							}
							// file Q has enough buffers to pick to send. go through the fileQ and send to client
							int missed_seq =3;
							for(i = 0; i< cwindow;i++)
							{
								struct fileSeg fseg = fileq_access(&fileQ, i);
								sendData.seqNum = fseg.seg;
								sendData.time_stamp = rtt_ts(&rttInfo);
								sendData.wSize = recvBuff.inflight;
								memcpy(sendData.udpdata, fseg.buff, 512);
								send(connectfd,&sendData, sizeof(struct packet_data),msg_flag);
								queue_add(&sendBuff,&sendData);

								do {
									recvd = recv(connectfd, &recvData, sizeof(struct packet_data),0);
									if(recvd > 0)
									{
										// if the file seg is successfully delivered, delete it from from fileQ
										fileq_delete(&fileQ, recvData.ackNum -1);
										if(queue_delete(&sendBuff,recvData.ackNum-1) <0)
										{
											missed_seq --;
										}
										else
										{
											missed_seq = 3;
											cwindow  = cwindow + 1;
										}
										if(missed_seq == 0)
										{
											// reduce the window size to half 
											cwindow = cwindow/2;
											break;
										}
									}
								}while(recvd > 0);
							}
							queue_purge(&sendBuff);
							
						}while(1);
						
					}
					else
					{
						// this is parent
					}
				}
				
			}
			
		}

	}
}
void sig_alarm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

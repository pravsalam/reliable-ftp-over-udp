#include<stdio.h>
#include"common_header.h"
#include<string.h>
int get_subaddr(char *ip, char* netmask, char* subaddr)
{
	int a,b,c,d;
	int k,l,m,n;
	int p,q,r,s;
	sscanf(ip,"%d.%d.%d.%d",&a,&b,&c,&d);
	sscanf(netmask, "%d.%d.%d.%d", &k,&l,&m,&n);
	p = a&k;
	q = b&l;
	r = c&m;
	s = d&n;
	sprintf(subaddr,"%d.%d.%d.%d", p,q,r,s);
	printf("subaddr = %s\n",subaddr);
	return 1;
}
int find_maxfd(struct sock_ifinfo ifinfo[], int len )
{
	int maxfd=0; 
	int i;
	for(i=0;i<len;i++)
	{
		if(maxfd < ifinfo[i].sockfd)
			maxfd = ifinfo[i].sockfd;
	}
	return maxfd;
}
int maxprefix_match(char *str1, char* str2)
{
	int i;
	int m = strlen(str1);
	int n = strlen(str2);
	for(i=0;i<m&&i<n; i++)
	{
		if(str1[i] != str2[i])
			break;
	}
	return (i-1);
}
int isAddress_local(struct sock_ifinfo ifinfo[], int len, char* ip, int *inf )
{
	int i;
	int max_matchfound=0;
	int local_match=0;
	for(i=0;i<len;i++)
	{
		local_match = maxprefix_match(ifinfo[i].ip_addr,ip);
		if(local_match > max_matchfound)
		{
			max_matchfound = local_match;
			*inf = i;
		}
	}
	return max_matchfound>0?1:0;

}

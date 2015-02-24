#include<stdio.h>
#include "common_header.h"
#define BUFFSIZE 20

struct cirQueue{
        int beg;
        int end;
		int inflight;
        struct packet_data buffer[BUFSIZE];
};
int queue_init(struct cirQueue *);
int queue_add(struct cirQueue *, struct packet_data* );
int queue_delete(struct cirQueue *, int seq);
int queue_availBuff(struct cirQueue *);
struct packet_data queue_first(struct cirQueue*);
int queue_collapse(struct cirQueue *);
int queue_half(struct cirQueue *);
int queue_purge(struct cirQueue *);
int queue_read(struct cirQueue *, struct packet_data*);
                                                                                                                                      

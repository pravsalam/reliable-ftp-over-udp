#include "buffer_queue.h"
int queue_init(struct cirQueue *queue)
{
	queue->beg = 0;
	queue->end = 0;
	queue->inflight = 0;
	return 0;
}
int queue_add(struct cirQueue *queue, struct packet_data *pack)
{
	if( ((queue->end +1)%BUFFSIZE) == queue->beg  )
	{
		printf(" buffer window is full\n");
		return -1; 
		
	}
	else 
	{
		queue->buffer[queue->end] = *pack;
		queue->end = (queue->end + 1)%BUFFSIZE;
		queue->inflight++;
		return 0;
	}
}
int queue_read(struct cirQueue *queue, struct packet_data *read_pack)
{
	if(queue->beg == queue->end)
	{
		// queue is empty
		return -1; 
	}
	else
	{
		//delete the first fellow 
		*read_pack = queue->buffer[queue->beg];
		queue->beg = (queue->beg +1)%BUFFSIZE;
		return 0;
	}
}
int queue_delete( struct cirQueue *queue, int seq )
{
	if(queue->beg == queue->end)
	{
		printf("buffer window is empty\n");
		return -1;
	}
	else if( (queue->buffer[queue->beg]).seqNum > seq)
	{
		return -1;
	}
	else
	{
		while( (queue->buffer[queue->beg]).seqNum <= seq )
		{
			//struct packet_data temp;
			//temp = queue->buffer[queue->beg];
			queue->beg = (queue->beg+1)%BUFFSIZE;
			queue->inflight--;
		}
	}
	return 0;
}
int queue_collapse(struct cirQueue *queue)
{
	queue->end = queue->beg+1;
	queue->inflight = 1;
	return 0;
}
int queue_half(struct cirQueue *queue)
{
	queue->end = (queue->beg+queue->inflight/2)%BUFFSIZE;
	queue->inflight = queue->inflight/2;
	return 0;
}
int queue_availBuff(struct cirQueue *queue)
{
	return BUFFSIZE - queue->inflight; 
}
int queue_purge(struct cirQueue *queue)
{
	queue->end = queue->beg;
	return 0;
}
#include "file_queue.h"
void fileq_init(struct fileSegQueue *fq)
{
	fq->beg = 0;
	fq->end = 0;
	fq->size = 0;
}
int fileq_add(struct fileSegQueue *fq, int seq, char *buff )
{
	if(fq->size <= FQSIZE)
	{
		(fq->fSeg[fq->end]).seg = seq;
		strcpy((fq->fSeg[fq->end]).buff, buff);
		fq->size++;
		fq->end = (fq->end+1)%FQSIZE;
		return 0;
	}
	else
	{
		printf(" queue overflow\n");
		return -1;
	}
}
int fileq_delete(struct fileSegQueue *fq, int offset)
{
	if(fq->size <0)
	{
		printf(" Queue underflow\n");
		return -1;
	}
	else{
		int i;
		for(i = fq->beg; i !=fq->end && fq->size >=0; i = (i+1)%FQSIZE)
		{
			if((fq->fSeg[i]).seg <= offset)
			{
				fq->beg ++;
				fq->size --;
			}
			else
				break;
		}
		return 0;

	}
}
struct fileSeg  fileq_access(struct fileSegQueue *fq, int offset)
{
	return 	fq->fSeg[fq->beg +offset];
}

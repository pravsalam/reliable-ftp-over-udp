#include<stdio.h>
#include<string.h>
#define FQSIZE 20
struct fileSeg{
	int seg;
	char buff[512];
};
struct fileSegQueue{
	int beg;
	int end;
	int size;
	struct fileSeg fSeg[FQSIZE];
};
void fileq_init(struct fileSegQueue *);
int fileq_add(struct fileSegQueue *, int seq, char * );
int fileq_delete(struct fileSegQueue *, int );
struct fileSeg  fileq_access(struct fileSegQueue *, int );
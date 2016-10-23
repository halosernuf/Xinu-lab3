/*  main.c  - main */
// 
#include <xinu.h>

#define NTP 256
#define MAXSUB 8
/*--------struct define start--------*/
//topic table struct
struct tpc
{
	struct subs* subsHead;
	int32 subsCount;
};
//topic subscriber list struct
struct subs
{
	pid32 pid;
	topic16 group;
	void (*hdlptr)(topic16, void*, uint32);
	struct subs* next;
};
struct tpc topictab[NTP];
sid32 tbsem;
//broker publish list struct
struct brlst
{
	uint32 size;
	void* data;
	topic16 topic;
	struct brlst* next;
};
struct brlst* brhead;
sid32 csm;
sid32 prd;
/*--------struct define end--------*/
status init_topictab(){
	int32 i;
	for(i=0;i<NTP;i++){
		topictab[i].subsCount=0;
		topictab[i].subsHead=(struct subs *)NULL;
	}
	if((tbsem=semcreate(1))==SYSERR) return SYSERR;
	return OK;
}

status init_broker(){
	brhead=(struct brlst *)getmem(sizeof(struct brlst));
	brhead->next=(struct brlst*)NULL;
	brhead->data=0;
	brhead->topic=0;
	if((prd=semcreate(MAXSUB))==SYSERR) return SYSERR;
	if((csm=semcreate(0))==SYSERR) return SYSERR;
	return OK;
}

syscall subscribe(topic16 topic, void (*handler)(topic16, void* ,uint32)){
	intmask mask;
	struct tpc* tpcentry;
	struct subs* subentry;
	mask=disable();
	/* return if wrong topic id*/
	if(topic&0x00FF<0 || topic&0x00FF>=NTP){
		restore(mask);
		return SYSERR;
	}
	/* return if topic subscriber is over 8*/
	tpcentry = &topictab[topic&0x00FF];
	wait(tbsem);
	if(tpcentry->subsCount>=MAXSUB){
		signal(tbsem);
		restore(mask);
		return SYSERR;
	}
	/* subscribe currpid to topic */
	subentry = tpcentry->subsHead;
	struct subs *newSub = (struct subs *)getmem(sizeof(struct subs));
	/* memory full*/
	if ((int32)newSub == SYSERR) {
		signal(tbsem);
		restore(mask);
		return SYSERR;
	}
	newSub->pid=currpid;
	newSub->hdlptr=handler;
	newSub->next=subentry;
	newSub->group=topic>>8;
	tpcentry->subsHead=newSub;
	signal(tbsem);
	restore(mask);
	return OK;
}
syscall unsubscribe(topic16 topic){
	intmask mask;
	struct tpc* tpcentry;
	struct subs* subentry;
	mask=disable();
	/* return if wrong topic id*/
	if(topic&0x00ff<0 || topic&0x00ff>=NTP){
		restore(mask);
		return SYSERR;
	}
	tpcentry = &topictab[topic&0x00ff];
	wait(tbsem);
	subentry=tpcentry->subsHead;
	/*traverse down the linkedList to get sub of currpid*/
	while(subentry!=(struct subs*)NULL && subentry->pid!=currpid){
		subentry=subentry->next;
	}
	/*currpid not in subs*/
	if(subentry==(struct subs*)NULL){
		signal(tbsem);
		restore(mask);
		return SYSERR;
	}
	/*delete subs*/
	if(subentry->next==(struct subs*)NULL){
		if(freemem(subentry,sizeof(struct subs))==SYSERR){
			signal(tbsem);
			restore(mask);
			return SYSERR;
		}
	}else{
		subentry->pid=subentry->next->pid;
		subentry->hdlptr=subentry->next->hdlptr;
		struct subs* tmp;
		tmp=subentry->next;
		subentry->next=subentry->next->next;
		if(freemem((char *)tmp,sizeof(struct subs))==SYSERR){
			signal(tbsem);
			restore(mask);
			return SYSERR;
		}
	}
	// signal(tpcentry->subsSem);
	signal(tbsem);
	restore(mask);
	return OK;
}
//publish topic entry to broker
syscall publish(topic16 topic, void* data, uint32 size){
	
	intmask mask;
	struct brlst* brentry;
	mask=disable();
	/* return if wrong topic id*/
	if(topic&0x00FF<0 || topic&0x00FF>=NTP){
		restore(mask);
		return SYSERR;
	}
	
	wait(prd);
	struct brlst *newBr = (struct brlst *)getmem(sizeof(struct brlst));
	if ((int32)newBr == SYSERR) {
		signal(prd);
		restore(mask);
		return SYSERR;
	}
	void *newData = getmem(sizeof(data)*size);
	if ((int32)newData == SYSERR) {
		signal(prd);
		restore(mask);
		return SYSERR;
	}
	memcpy(newData,data,sizeof(data)*size);//copy data from stack to heap
	newBr->data=newData;
	newBr->size=size;
	newBr->topic=topic;
	newBr->next=(struct brlst*)NULL;
	brentry=brhead;
	while(brentry->next!=(struct brlst*)NULL){
		brentry=brentry->next;
	}
	brentry->next=newBr;
	if(brhead->next!=(struct brlst*)NULL){
		printf("brhead->next->topic 0x%04x \n", brhead->next->topic);
	}
	signal(csm);
	restore(mask);
	return OK;
}

void handler1(topic16 topic,void* data, uint32 size){
	char *cstData=(char*)data;
	printf("- Function handler1() is called with topic16 0x%04x and data %s\n",topic&0xffff,cstData);
	
}

void handler2(topic16 topic,void* data, uint32 size){
	char *cstData=(char*)data;
	printf("- Function handler2() is called with topic16 0x%04x and data %s\n",topic&0xffff,cstData);
}

syscall unsubscribeAll(){
	int32 i;
	for(i=0;i<NTP;i++){
		unsubscribe(i);
	}
	return OK;
}

process A(){
	printf("process A start\n");
	topic16 topic;
	topic=0x013F;
	if(subscribe(topic,&handler1)==SYSERR){
		printf("fail to subscribe\n");
	}else{
		printf("process A subscribe to 0x%04x with handler1\n",topic);
	}
	
	sleep(20);
	unsubscribeAll();
	return OK;
}
process B(){
	printf("process B start\n");
	topic16 topic;
	topic=0x023F;
	if(subscribe(topic,&handler2)==SYSERR){
		printf("fail to subscribe\n");
	}else{
		printf("process B subscribe to 0x%04x with handler2\n",topic);
	}
	
	sleep(20);
	unsubscribeAll();
	return OK;
}

process C(){
	printf("process C start\n");
	sleep(1);
	topic16 topic;
	char data[15]="hello world";
	uint32 size = sizeof(data)/sizeof(data[0]);
	topic=0x013F;
	// data=0xFF;
	printf("Process C publishes data %s to topic16 0x%04x\n",data,topic);
	publish(topic,data,size);
	return OK;
}

process D(){
	printf("process D start\n");
	sleep(1);
	topic16 topic;
	char data[15]="go gators!";
	uint32 size = sizeof(data)/sizeof(data[0]);
	topic=0x003F;
	printf("Process D publishes data %s to topic16 0x%04x\n",data,topic);
	publish(topic,data,size);
	return OK;
}


process Broker(){
	struct brlst* brentry;
	struct subs* subsentry;
	printf("Broker start\n");
	while(1){
		wait(csm);
		brentry=brhead->next;
		wait(tbsem);
		subsentry=topictab[brentry->topic & 0x00FF].subsHead;
		while(subsentry!=(struct subs *)NULL){
			if(brentry->topic>>8!=0 && subsentry->group!=brentry->topic>>8){
				subsentry=subsentry->next;
				continue;
			}
			subsentry->hdlptr(brentry->topic,brentry->data,brentry->size);
			subsentry=subsentry->next;
		}
		signal(tbsem);
		brhead->next=brentry->next;
		freemem(brentry,sizeof(struct brlst));
		signal(prd);
		yield();
	}
	return OK;
}

process	main(void)
{
	recvclr();
	
	if(init_topictab()==SYSERR){
		printf("fail to init topic table");
		return SYSERR;
	}
	if(init_broker()==SYSERR){
		printf("fail to init broker list");
		return SYSERR;
	}
	
	resume(create(A, 4096, 50, "A", 0));
	resume(create(B, 4096, 50, "B", 0));
	resume(create(C, 4096, 50, "C", 0));
	resume(create(D, 4096, 50, "D", 0));
	resume(create(Broker,4096, 50, "Broker", 0));
	printf("finished all process\n");
	return OK;
}

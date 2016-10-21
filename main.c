/*  main.c  - main */
// 
#include <xinu.h>

#define NTP 256
#define MAXSUB 8
#define MAXBLK 20
#define NIL (struct subs* )0x00
/*--------struct define start--------*/
//topic table struct
struct tpc
{
	struct subs* subsHead;
	int32 subsCount;
	// sid32 subsSem;
};
//topic subscriber list struct
struct subs
{
	pid32 pid;
	void (*hdlptr)(topic16, uint32);
	struct subs* next;
};
struct tpc topictab[NTP];
sid32 tbsem;
//broker publish list struct
struct brlst
{
	// void (*hdlptr)(topic16, uint32);
	// topic16 topic;
	uint32 data;
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
	if((prd=semcreate(1))==SYSERR) return SYSERR;
	if((csm=semcreate(0))==SYSERR) return SYSERR;
	return OK;
}

syscall subscribe(topic16 topic, void (*handler)(topic16, uint32)){
	intmask mask;
	struct tpc* tpcentry;
	struct subs* subentry;
	mask=disable();
	/* return if wrong topic id*/
	if(topic<0 || topic>=NTP){
		restore(mask);
		return SYSERR;
	}
	/* return if topic subscriber is over 8*/
	tpcentry = &topictab[topic];
	// wait(tpcentry->subsSem);
	wait(tbsem);
	if(tpcentry->subsCount>=MAXSUB){
		signal(tbsem);
		restore(mask);
		return SYSERR;
	}
	/* subscribe currpid to topic */
	subentry = tpcentry->subsHead;
	// struct subs *newSub = (struct subs *)malloc(sizeof(struct subs));
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
	tpcentry->subsHead=newSub;
	// signal(tpcentry->subsSem);
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
	if(topic<0 || topic>=NTP){
		restore(mask);
		return SYSERR;
	}
	tpcentry = &topictab[topic];
	// wait(tpcentry->subsSem);
	wait(tbsem);
	subentry=tpcentry->subsHead;
	/*traverse down the linkedList to get sub of currpid*/
	while(subentry!=(struct subs*)NULL && subentry->pid!=currpid){
		subentry=subentry->next;
	}
	/*currpid not in subs*/
	if(subentry==(struct subs*)NULL){
		// signal(tpcentry->subsSem);
		signal(tbsem);
		restore(mask);
		return SYSERR;
	}
	/*delete subs*/
	if(subentry->next==(struct subs*)NULL){
		if(freemem(subentry,sizeof(struct subs))==SYSERR){
		 // if(free(subentry,sizeof(struct subs))==SYSERR){
			// signal(tpcentry->subsSem);
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
			// signal(tpcentry->subsSem);
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
syscall publish(topic16 topic, uint32 data){
	
	intmask mask;
	// struct tpc* tpcentry;
	// struct subs* subentry;
	struct brlst* brentry;
	mask=disable();
	printf("publish %d with data %d \n", topic,data);
	/* return if wrong topic id*/
	if(topic<0 || topic>=NTP){
		restore(mask);
		return SYSERR;
	}
	wait(tbsem);
	wait(prd);
	printf("copy data to broker blk\n");
	struct brlst *newBr = (struct brlst *)getmem(sizeof(struct brlst));
	newBr->data=data;
	newBr->topic=topic;
	newBr->next=(struct brlst*)NULL;
	brentry=brhead;
	while(brentry->next!=(struct brlst*)NULL){
		printf("brentry->next->topic %d \n", brentry->next->topic);
		brentry=brentry->next;
	}
	brentry->next=newBr;
	if(brhead->next!=(struct brlst*)NULL){
		printf("brhead->next->topic %d \n", brhead->next->topic);
	}
	signal(csm);
	signal(tbsem);
	restore(mask);
	return OK;
}

void handler1(topic16 topic,uint32 data){
	printf("- Function handler1() is called with arguments %d and %d\n",topic,data);
}

void handler2(topic16 topic,uint32 data){
	printf("- Function handler2() is called with arguments %d and %d\n",topic,data);
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
	if(subscribe(1,&handler1)==SYSERR){
		printf("fail to subscribe\n");
	}else{
		printf("process %d subscribe to %d with handler1\n",currpid,1);
	}
	if(subscribe(2,&handler2)==SYSERR){
		printf("fail to subscribe\n");
	}else{
		printf("process %d subscribe to %d with handler2\n",currpid,2);
	}
	if(subscribe(30,&handler1)==SYSERR){
		printf("fail to subscribe\n");
	}else{
		printf("process %d subscribe to %d with handler1\n",currpid,30);
	}
	sleep(1);
		// publish(2,1);
	// printf("finish publish\n");
	// sleep(10);
	// unsubscribeAll();
	return OK;
}
process B(){
	printf("process B start\n");
	sleep(5);
	publish(1,100);
	// sleep(10);
	publish(2,200);
	// sleep(10);
	publish(30,300);
	// sleep(10);
	printf("finish publish\n");
	sleep(1);
	// unsubscribeAll();
	return OK;
}



process Broker(){
	struct brlst* brentry;
	struct subs* subsentry;
	printf("Broker start\n");
	while(1){
		wait(csm);
		printf("broker loop\n");
		if(brhead->next!=(struct brlst *)NULL){
			printf("get broker list locker\n");
			printf("broker run handler\n");
			brentry=brhead->next;
			wait(tbsem);
			subsentry=topictab[brentry->topic].subsHead;
			while(subsentry!=(struct subs *)NULL){
				subsentry->hdlptr(brentry->topic,brentry->data);
				subsentry=subsentry->next;
			}
			signal(tbsem);
			brhead->next=brentry->next;
			freemem(brentry,sizeof(struct brlst));
		}
		signal(prd);
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
	resume(create(Broker,4096, 50, "Broker", 0));
	printf("finished all process\n");
	return OK;
}

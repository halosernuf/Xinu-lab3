/*  main.c  - main */
// 
#include <xinu.h>

#define NTP 256
#define MAXSUB 8
#define MAXBLK 20
#define NIL (struct subs* )0x00
struct tpc
{
	struct subs* subsHead;
	int32 subsCount;
	// sid32 subsSem;
};
struct subs
{
	pid32 pid;
	void (*hdlptr)(topic16, uint32);
	struct subs* next;
};

struct tpc topictab[NTP];
sid32 tbsem;

struct brblk
{
	void (*hdlptr)(topic16, uint32);
	topic16 topic;
	uint32 data;
};
struct brblk br[MAXBLK];
int32 head=0;
int32 tail=0;
sid32 prd;
sid32 csm;
status init_topictab(){
	int32 i;
	for(i=0;i<NTP;i++){
		topictab[i].subsCount=0;
		topictab[i].subsHead=(struct subs *)NULL;
		// if((topictab[i].subsSem=semcreate(1))==SYSERR){
		// 	return SYSERR;
		// };
	}
	if((tbsem=semcreate(1))==SYSERR) return SYSERR;
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
syscall publish(topic16 topic, uint32 data){
	printf("publish %d with data %d \n", topic,data);
	intmask mask;
	struct tpc* tpcentry;
	struct subs* subentry;
	struct brblk* brentry;
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
	while(subentry!=(struct subs*)NULL){
		wait(prd);
		printf("copy data to broker blk\n");
		brentry=&br[head];
		brentry->topic=topic;
		brentry->hdlptr=subentry->hdlptr;
		brentry->data=data;
		head=(head+1)%MAXBLK;
		subentry=subentry->next;
		signal(csm);
	}
	// signal(tpcentry->subsSem);
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
	printf("process A start");
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
	sleep(40);
	// publish(2,1);
	// printf("finish publish\n");
	// sleep(10);
	unsubscribeAll();
	return OK;
}
process B(){
	printf("process B start");
	publish(1,100);
	publish(2,200);
	publish(30,300);
	printf("finish publish\n");
	sleep(10);
	unsubscribeAll();
	return OK;
}
process Broker(){
	struct brblk* brentry;
	printf("Broker start\n");
	while(1){
		wait(csm);
		printf("run callback\n");
		brentry=&br[tail];
		brentry->hdlptr(brentry->topic,brentry->data);
		tail=(tail+1)%MAXBLK;
		signal(prd);
	}
	return OK;
}
process	main(void)
{
	recvclr();
	prd=semcreate(MAXBLK);
	csm=semcreate(0);
	if(init_topictab()==SYSERR){
		printf("fail to init");
		return SYSERR;
	}
	resume(create(A, 4096, 50, "A", 0));
	resume(create(B, 4096, 50, "B", 0));
	resume(create(Broker,4096, 50, "Broker", 0));
	printf("finished all process\n");
	return OK;
}

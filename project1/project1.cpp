#include <winsock2.h>
#include <stdio.h>
#include <WS2tcpip.h>
#include <time.h>
#include <thread>
#include <mutex>
#include <chrono>
//#include "tinycthread.h" //<>先在系统目录中查找，而""则是先在工程所在的目录中寻找
#include "es_TIMER.H"
#pragma comment(lib,"Ws2_32.lib") //引入外部的lib文件，vs里本身没有

int i,num,pktsize,seq;
int pkts = 0;
unsigned long int len = 0;
double lostrate = 0;
int lostnum = 0;
long double rate = 0;
int flag = 0;
double duration = 0;
double olduration = 0;
double jitter = 0;
std::mutex m;
std::unique_lock<std::mutex> lck(m, std::defer_lock);

void PrintUsage(){
	printf("invalid command line\n");
	printf("Usage:-------------------------------------------------------------------------------------------\n");
	printf("      NetProbe s [refresh_interval] [remote_host] [remote_port] [protocol] [Pktsize] [rate] [num]\n");
	printf("      NetProbe r [refresh_interval] [local_host] [local_port] [protocol] [Pktsize]\n");
	printf("      NetProbe h [host_name]\n");
	printf("      -------------------------------------------------------------------------------------------\n");
}

int Hostinfo(char*Host_name){
	int i=0;
	int num;
	char*Ipaddress;
	hostent*remoteHost;
	if(isalpha(Host_name[0])){
		remoteHost=gethostbyname(Host_name);
		if(WSAGetLastError()!=0){
			if(WSAGetLastError()==11001){
				printf("Host not found...\n");
			}
			else{
				printf("Error,code:%i\n",WSAGetLastError());
			}
			return -1;
		}
		num=sizeof(remoteHost->h_addr_list);
		struct in_addr Addr;
		memmove(&Addr,remoteHost->h_addr_list[1],4);
		Ipaddress=inet_ntoa(*(struct in_addr*)remoteHost->h_addr_list);
		printf("Official name: %s\n",remoteHost->h_name);
		while (remoteHost->h_addr_list[i] != 0)
		{
			printf("IP Address(es): %s\n",inet_ntoa(*(struct in_addr *)remoteHost->h_addr_list[i]));
			i++;
		}
		return 0;
	}
	else{
		PrintUsage();
		exit(-1);
	}
}

int displayS(int interval)
{
	while(1)
	{
		lck.lock();
		printf("Elapsed[%.1fs] Pkts[%d] Rate[%.1fMBps] Jitter[%.1fms]\n",duration,pkts,rate,jitter);
		if (flag == 1) break;
		lck.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	lck.unlock();
	return 0;
}

int displayR(int interval)
{
	while (1)
	{
		lck.lock();
		printf("Elapsed[%.1fs] Pkts[%d] Lost[%d, %.1f%%] Rate[%.1fMBps] Jitter[%.1fms]\n", duration, pkts,lostnum,lostrate,rate,jitter);
		if (flag == 1) break;
		lck.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	lck.unlock();
	return 0;
}

int TCP_Send(char*Interval,char*Host_IP,char*Port,char*Pktsize,char*Rate,char*Num)
{
	int interval = atoi(Interval);
	double limitrate =atoi(Rate);
	int sleeptime = 0;
	num=atoi(Num);
	pktsize=atoi(Pktsize);
	i=1;

	printf("Creating Sender socket . . .\n");
	SOCKET sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sockfd==-1) {
		printf("Socket Error!\n");
		return -1;  //error
	}
	struct sockaddr_in peerAddr;
	peerAddr.sin_family = AF_INET;
	peerAddr.sin_addr.s_addr=inet_addr(Host_IP);
	peerAddr.sin_port=htons(atoi(Port));

	if(connect(sockfd,(struct sockaddr*)&peerAddr,sizeof(peerAddr))==SOCKET_ERROR){
		printf("connect() failed.Error code:%i\n",WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}
	printf("successfully connected\n");
	char*tmp=(char*)malloc(sizeof(char)*(pktsize+8)); //sizeof不能查看动态分配的内存！返回的是指针的大小，都是4
	memset(tmp,1,sizeof(char)*(pktsize+8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayS, interval); //创建display thread

	if(num!=0){
		while (i <= num){
			int r;
			int bytesent = 0;
			memcpy(tmp, &num, sizeof(num));
			memcpy(tmp + 4, &i, sizeof(i));
			r = send(sockfd, tmp, pktsize + 8, 0);
			bytesent = r;
			if (r == SOCKET_ERROR){
				printf("send failed.Error code:%i\n", WSAGetLastError());
				lck.lock();
				flag = 1;
				lck.unlock();
				th.join();
				closesocket(sockfd);
				return -1;
			}
			while (bytesent != pktsize + 8){
				r = send(sockfd, tmp + bytesent, pktsize + 8 - bytesent, 0);
				if (r == SOCKET_ERROR){
					printf("send failed.Error code:%i\n", WSAGetLastError());
					lck.lock();
					flag = 1;
					lck.unlock();
					th.join();
					closesocket(sockfd);
					return -1;
				}
				bytesent = bytesent + r;
			}
			//printf("send seq:%d\n",i);
			lck.lock();
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			jitter = (jitter*(i - 1) +fabs((duration - olduration - (duration / i)))*1000) / i;
			olduration = duration;
			if (limitrate > 0)
			{
				sleeptime =((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime>0) {
					Sleep(sleeptime);
				}
			}

			lck.unlock();
			i++;
		}
	}
	else{
		while(1){
			int r;
			int bytesent=0;
			memcpy(tmp,&num,sizeof(num));
			memcpy(tmp+4,&i,sizeof(i));
			r=send(sockfd,tmp,pktsize+8,0);
			bytesent=r;
			if(r==SOCKET_ERROR){
				printf("send failed.Error code:%i\n",WSAGetLastError());
				lck.lock();
				flag = 1;
				lck.unlock();
				th.join();
				closesocket(sockfd);
				return -1;
			}
			while(bytesent!=pktsize+8){
				r=send(sockfd,tmp+bytesent,pktsize+8-bytesent,0);
				if(r==SOCKET_ERROR){
					printf("send failed.Error code:%i\n",WSAGetLastError());
					lck.lock();
					flag = 1;
					lck.unlock();
					th.join();
					closesocket(sockfd);
					return -1;
				}
				bytesent=bytesent+r;
			} 
			//printf("send seq:%d\n",i);
			lck.lock();
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			jitter = (jitter*(i - 1) + fabs((duration - olduration - (duration / i))) * 1000) / i;
			olduration = duration;
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration)*1000 ;
				if (sleeptime>0) {
					Sleep(sleeptime);
				}
			}

			lck.unlock();
			i++;
		}
	}
	
	lck.lock();
	flag = 1;
	lck.unlock();

	th.join();
	closesocket(sockfd);
	return 0;
}

int TCP_Recv(char*Interval,char*Host_IP, char*Port, char*Pktsize)
{
	int interval = atoi(Interval);
	int tmpkts=0;
	int oldpkts = 0;
	seq=0;
	i = 0;
	pktsize=atoi(Pktsize);
	num=0;

	printf("Creating Receiver socket . . .\n");
	SOCKET sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	if(sockfd==-1) {
		printf("Socket Error!\n");
		return -1;  //error
	}
	struct sockaddr_in MyAddr,peerAddr;
	MyAddr.sin_family = AF_INET;
	MyAddr.sin_addr.s_addr=inet_addr(Host_IP);
	MyAddr.sin_port=htons(atoi(Port));

	if(bind(sockfd,(struct sockaddr *)&MyAddr,sizeof(MyAddr))==SOCKET_ERROR){
		printf("bind failed.Error code:%i\n",WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}
	printf("successfully binded\n");
	listen(sockfd,5);
	int len=sizeof(peerAddr);
	SOCKET newsockfd=accept(sockfd,(struct sockaddr *)&peerAddr,&len);
	char*tmp=(char*)malloc(sizeof(char)*(pktsize+8));
	memset(tmp,0,sizeof(char)*(pktsize+8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayR, interval);

	while(1){
		int buflen=recv(newsockfd,tmp,pktsize+8,0);		
		if(buflen==SOCKET_ERROR){
			printf("send failed.Error code:%i\n",WSAGetLastError());
			lck.lock();
			flag = 1;
			lck.unlock();
			th.join();
			closesocket(sockfd);
			return -1;
		}
		
		if (buflen == 0)
		{
			
			break;
		  
		}
		memcpy(&num,tmp,4);
		memcpy(&seq,tmp+4,4);
		len = len + buflen;
	
		//printf("total Bytes:%d\n", len);
		//printf("pkts:%d\n", len / (pktsize + 8));
		//printf("seq:%d\n", seq);
		
		i++;
		lck.lock();
		pkts = tmpkts + ((unsigned long int)len / (pktsize + 8));
		duration = (double)(es1.Elapsed()) / 1000;
		rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
		jitter = (jitter*(i - 1) + fabs((duration - olduration - (duration / i))) * 1000) / i;
		olduration = duration;
		oldpkts = pkts;
		lck.unlock();
			
		
		if (len >= 400000000)
		{
			tmpkts = pkts;
			len = len % (pktsize + 8);
		}
		
		
	}
	
	lck.lock();
	flag = 1;
	lck.unlock();

	th.join();
	closesocket(newsockfd);
	closesocket(sockfd);
	return 0;
}

int UDP_Send(char*Interval,char*Host_IP,char*Port,char*Pktsize,char*Rate,char*Num)
{
	int interval = atoi(Interval);
	double limitrate = atoi(Rate);
	int sleeptime = 0;
	pktsize=atoi(Pktsize);
	num=atoi(Num);
	i=1;

	printf("Creating Sender socket . . .\n");
	SOCKET sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(sockfd==-1) {
		printf("Socket Error!\n");
		return -1;  //error
	}
	printf("successfully created\n");
	struct sockaddr_in peerAddr;
	peerAddr.sin_family = AF_INET;
	peerAddr.sin_addr.s_addr=inet_addr(Host_IP);
	peerAddr.sin_port=htons(atoi(Port));

	char*tmp=(char*)malloc(sizeof(char)*(pktsize+8));
	memset(tmp,1,sizeof(char)*(pktsize+8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayS, interval);

	if(num!=0){
		while(i<=num){
			memcpy(tmp,&num,sizeof(num));
			memcpy(tmp+4,&i,sizeof(i));

			if(sendto(sockfd,tmp,pktsize+8,0,(struct sockaddr*)&peerAddr,sizeof(peerAddr))==SOCKET_ERROR){
				printf("send failed.Error code:%i\n",WSAGetLastError());
				lck.lock();
				flag = 1;
				lck.unlock();
				th.join();
				closesocket(sockfd);
				return -1;
			}
			//printf("send seq:%d\n",i);
			lck.lock();
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			jitter = (jitter*(i - 1) + fabs((duration - olduration - (duration / i))) * 1000) / i;
			olduration = duration;
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime>0) {
					Sleep(sleeptime);
				}
			}

			lck.unlock();
			i++;
		}
	}
	else{
		while(1){
			memcpy(tmp,&num,sizeof(num));
			memcpy(tmp+4,&i,sizeof(i));

			if(sendto(sockfd,tmp,pktsize+8,0,(struct sockaddr*)&peerAddr,sizeof(peerAddr))==SOCKET_ERROR){
				printf("send failed.Error code:%i\n",WSAGetLastError());
				lck.lock();
				flag = 1;
				lck.unlock();
				th.join();
				closesocket(sockfd);
				return -1;
			}
			//printf("send seq:%d\n",i);
			lck.lock();
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			jitter = (jitter*(i - 1) + fabs((duration - olduration - (duration / i))) * 1000) / i;
			olduration = duration;
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime>0) {
					Sleep(sleeptime);
				}
			}

			lck.unlock();
			i++;
		}
	}
	lck.lock();
	flag = 1;
	lck.unlock();
	th.join();
	closesocket(sockfd);
	return 0;
}

int UDP_Recv(char*Interval,char*Host_IP,char*Port,char*Pktsize)
{
	int interval = atoi(Interval);
	int len=sizeof(sockaddr_in);
	pktsize=atoi(Pktsize);
	num=0;
	i=0;
	seq=0;

	printf("Creating Receiver socket . . .\n");
	SOCKET sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if(sockfd==-1) {
		printf("Socket Error!\n");
		return -1;  //error
	}
	struct sockaddr_in MyAddr,peerAddr;
	MyAddr.sin_family = AF_INET;
	MyAddr.sin_addr.s_addr=inet_addr(Host_IP);
	MyAddr.sin_port=htons(atoi(Port));

	if(bind(sockfd,(struct sockaddr *)&MyAddr,sizeof(MyAddr))==SOCKET_ERROR){
		printf("bind failed.Error code:%i\n",WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}
	printf("successfully binded\n");
	char*tmp=(char*)malloc(sizeof(char)*(pktsize+8));
	memset(tmp,0,sizeof(char)*(pktsize+8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayR, interval);

	while(1){
		int buflen=recvfrom(sockfd,tmp,pktsize+8,0,(struct sockaddr*)&peerAddr,&len);		
		if(buflen==SOCKET_ERROR){
			printf("send failed.Error code:%i\n",WSAGetLastError());
			lck.lock();
			flag = 1;
			lck.unlock();
			th.join();
			closesocket(sockfd);
			return -1;
		}
		memcpy(&num,tmp,4);
		memcpy(&seq,tmp+4,4);
		//printf("recv seq:%d\n",seq);
		i++;//最终i的值是收到pkts的个数，num-i就是lost的packet的个数

		lck.lock();
		pkts = i;
		duration = (double)(es1.Elapsed()) / 1000;
		rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
		lostrate = ((seq - pkts )/(double) seq)*100;
		lostnum = seq - pkts;
		jitter = (jitter*(i - 1) + fabs((duration - olduration - (duration / i))) * 1000) / i;
		olduration = duration;
		lck.unlock();

		if(seq==num){
			break;
		}
	}
	lck.lock();
	flag = 1;
	lck.unlock();
	th.join();
	closesocket(sockfd);
	return 0;
}

int main(int argc,char*argv[])
{
	WSADATA wsaData;//指向WSADATA结构的指针，该结构用来存放windows socket的初始化信息
	int Ret;
	// Initialize Winsock version 2.2

	if ((Ret = WSAStartup(MAKEWORD(2,2), &wsaData)) != 0)  //0 success
	{
		// NOTE: Since Winsock failed to load we cannot use 
		// WSAGetLastError to determine the specific error for
		// why it failed. Instead we can rely on the return 
		// status of WSAStartup：WSASYSNOTREADY，WSAVERNOTSUPPORTED，WSAEINVAL

		printf("WSAStartup failed with error %d\n", Ret);
		return 0;
	}

	// Setup Winsock communication code here 
	if(argc!=3&&argc!=7&&argc!=9){
		PrintUsage();
		exit(-1);
	}
	if((strcmp(argv[1],"s")==0)&&(strcmp(argv[5],"tcp")==0)){
		if(TCP_Send(argv[2],argv[3],argv[4],argv[6],argv[7],argv[8])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if((strcmp(argv[1],"s")==0)&&(strcmp(argv[5],"udp")==0)){
		if(UDP_Send(argv[2],argv[3],argv[4],argv[6],argv[7],argv[8])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if((strcmp(argv[1],"r")==0)&&(strcmp(argv[5],"tcp")==0)){
		if(TCP_Recv(argv[2],argv[3],argv[4],argv[6])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if((strcmp(argv[1],"r")==0)&&(strcmp(argv[5],"udp")==0)){
		if(UDP_Recv(argv[2],argv[3],argv[4],argv[6])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if((strcmp(argv[1],"h")==0)){
		if(Hostinfo(argv[2])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else{
		PrintUsage();
		exit(-1);
	}
	// When your application is finished call WSACleanup
	if (WSACleanup() == SOCKET_ERROR)
	{
		printf("WSACleanup failed with error %d\n", WSAGetLastError());
	}
	return 0;
}
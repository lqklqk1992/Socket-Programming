#ifdef WIN32 //Windows
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <time.h>
#pragma comment(lib,"Ws2_32.lib") 
#else  //linux
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#define SOCKET int
#define SOCKET_ERROR -1
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define Sleep(s) usleep(s*1000)
#define ioctlsocket ioctl
#define WSAWOULDBLOCK EWOULDBLOCK
#endif
#include <thread>
#include <mutex>
#include "es_TIMER.H"

std::mutex m; //for client:lock all ; for server:lock countTCP
std::mutex n; //for server:lock countUDP
std::mutex l; //for server:lock ratearray[5]
std::mutex o; //for server:lock jobnumber


int pkts = 0;
unsigned long int len = 0;
double lostrate = 0;
int lostnum = 0;
double rate = 0;
int flag = 0;
double duration = 0;
int countTCP = 0;
int countUDP = 0;
double ratearray[10];
int jobnumber = 0;


void PrintUsage(){
	printf("invalid command line\n");
	printf("Usage:-------------------------------------------------------------------------------------------\n");
	printf("      NetProbe s [refresh_interval] [server_host] [server_port] [protocol] [Pktsize] [rate] [num]\n");
	printf("      NetProbe c [refresh_interval] [tcp_port] [udp_port]\n");
	printf("      -------------------------------------------------------------------------------------------\n");
}

int displayS(char*Interval)
{
	int interval = atoi(Interval);
	while (1)
	{
		m.lock();
		n.lock();
		l.lock();
		rate = ratearray[0] + ratearray[1] + ratearray[2] + ratearray[3] + ratearray[4] + ratearray[5] + ratearray[6] + ratearray[7] + ratearray[8] + ratearray[9];
		printf("Aggregate Rate[%.1fMBps] # of TCP Clients[%d] # of UDP Clients[%d]\n", rate, countTCP, countUDP);
		m.unlock();
		n.unlock();
		l.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	return 0;
}

int displayC(int interval)
{
	while (1)
	{
		m.lock();
		printf("Elapsed[%.1fs] Pkts[%d] Lost[%d, %.1f%%] Rate[%.1fMBps]\n", duration, pkts, lostnum, lostrate, rate);
		if (flag == 1) break;
		m.unlock();
		std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	}
	m.unlock();
	return 0;
}

int TCP_thread(SOCKET newsockfd,int Jobnumber){
	int jobnumber = Jobnumber;
	double duration = 0;
	int pkts = 0;
	int num, pktsize, limitrate,sleeptime;
	int i = 1;
	char*tmp = (char*)malloc(sizeof(char)* 12);
	memset(tmp, 0, sizeof(char)* 12);

	if (recv(newsockfd, tmp, 12, 0) == SOCKET_ERROR){
		printf("TCP request receiving failed.Error code:%i\n", WSAGetLastError());
		closesocket(newsockfd);
		m.lock();
		countTCP--;
		m.unlock();
		return -1;
	}
	memcpy(&num, tmp, 4);
	memcpy(&pktsize, tmp + 4, 4);
	memcpy(&limitrate, tmp + 8, 4);

	char*sendbuf = (char*)malloc(sizeof(char)*(pktsize + 8));
	memset(sendbuf, 0, sizeof(char)*(pktsize + 8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();

	if (num != 0){
		while (i <= num){
			int r;
			int bytesent = 0;
			memcpy(sendbuf, &num, sizeof(num));
			memcpy(sendbuf + 4, &i, sizeof(i));
			r = send(newsockfd, sendbuf, pktsize + 8, 0);
			bytesent = r;
			if (r == SOCKET_ERROR){
				printf("send failed.Error code:%i\n", WSAGetLastError());
				closesocket(newsockfd);
				m.lock();
				countTCP--;
				m.unlock();
				l.lock();
				ratearray[jobnumber - 1] = 0;
				l.unlock();
				return -1;
			}
			while (bytesent != pktsize + 8){
				r = send(newsockfd, sendbuf + bytesent, pktsize + 8 - bytesent, 0);
				if (r == SOCKET_ERROR){
					printf("send failed.Error code:%i\n", WSAGetLastError());
					closesocket(newsockfd);
					m.lock();
					countTCP--;
					m.unlock();
					l.lock();
					ratearray[jobnumber - 1] = 0;
					l.unlock();
					return -1;
				}
				bytesent = bytesent + r;
			}
			//printf("send seq:%d\n",i);
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			l.lock();
			ratearray[jobnumber-1] = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			l.unlock();
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime > 0) {
					Sleep(sleeptime);
				}
			}
			i++;
		}
	}
	else{
		while (1){
			int r;
			int bytesent = 0;
			memcpy(sendbuf, &num, sizeof(num));
			memcpy(sendbuf + 4, &i, sizeof(i));
			r = send(newsockfd, sendbuf, pktsize + 8, 0);
			bytesent = r;
			if (r == SOCKET_ERROR){
				printf("send failed.Error code:%i\n", WSAGetLastError());
				closesocket(newsockfd);
				m.lock();
				countTCP--;
				m.unlock();
				l.lock();
				ratearray[jobnumber - 1] = 0;
				l.unlock();
				return -1;
			}
			while (bytesent != pktsize + 8){
				r = send(newsockfd, sendbuf + bytesent, pktsize + 8 - bytesent, 0);
				if (r == SOCKET_ERROR){
					printf("send failed.Error code:%i\n", WSAGetLastError());
					closesocket(newsockfd);
					m.lock();
					countTCP--;
					m.unlock();
					l.lock();
					ratearray[jobnumber - 1] = 0;
					l.unlock();
					return -1;
				}
				bytesent = bytesent + r;
			}
			//printf("send seq:%d\n",i);
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			l.lock();
			ratearray[jobnumber - 1] = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			l.unlock();
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime > 0) {
					Sleep(sleeptime);
				}
			}
			i++;
		}
	}
	closesocket(newsockfd);
	m.lock();
	countTCP--;
	m.unlock();
	l.lock();
	ratearray[jobnumber - 1] = 0;
	l.unlock();
	return 0;
}

int UDP_thread(sockaddr_in peerAddr, char*tmp,int Jobnumber){
	int jobnumber = Jobnumber;
	double duration = 0;
	int pkts = 0;
	int num, pktsize, limitrate,sleeptime;
	int i = 1;

	SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	memcpy(&num, tmp, 4);
	memcpy(&pktsize, tmp + 4, 4);
	memcpy(&limitrate, tmp + 8, 4);
	char*sendbuf = (char*)malloc(sizeof(char)*(pktsize + 8));
	memset(sendbuf, 0, sizeof(char)*(pktsize + 8));

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();

	if (num != 0){
		while (i <= num){
			memcpy(sendbuf, &num, sizeof(num));
			memcpy(sendbuf + 4, &i, sizeof(i));

			if (sendto(sockfd, sendbuf, pktsize + 8, 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
				printf("send failed.Error code:%i\n", WSAGetLastError());
				closesocket(sockfd);
				n.lock();
				countUDP--;
				n.unlock();
				l.lock();
				ratearray[jobnumber - 1] = 0;
				l.unlock();
				return -1;
			}
			//printf("send seq:%d\n",i);
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			l.lock();
			ratearray[jobnumber - 1] = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			l.unlock();
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime > 0) {
					Sleep(sleeptime);
				}
			}
			i++;
		}
	}
	else{
		while (1){
			memcpy(sendbuf, &num, sizeof(num));
			memcpy(sendbuf + 4, &i, sizeof(i));

			if (sendto(sockfd, sendbuf, pktsize + 8, 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
				printf("send failed.Error code:%i\n", WSAGetLastError());
				closesocket(sockfd);
				n.lock();
				countUDP--;
				n.unlock();
				l.lock();
				ratearray[jobnumber - 1] = 0;
				l.unlock();
				return -1;
			}
			//printf("send seq:%d\n",i);
			pkts++;
			duration = (double)(es1.Elapsed()) / 1000;
			l.lock();
			ratearray[jobnumber - 1] = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
			l.unlock();
			if (limitrate > 0)
			{
				sleeptime = ((((long double)pkts*pktsize) / limitrate) - duration) * 1000;
				if (sleeptime > 0) {
					Sleep(sleeptime);
				}
			}
			i++;
		}
	}
	closesocket(sockfd);
	n.lock();
	countUDP--;
	n.unlock();
	l.lock();
	ratearray[jobnumber - 1] = 0;
	l.unlock();
	return 0;
}

int TCP_Client(char*Interval,char*Host_IP,char*Port,char*Pktsize,char*Rate,char*Num)
{
	int num, pktsize,seq;
	int tmpkts = 0;
	int interval = atoi(Interval);
	int limitrate =atoi(Rate);
	pkts = 0;
	num=atoi(Num);
	pktsize=atoi(Pktsize);
	
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
	char*tmp=(char*)malloc(sizeof(char)*12);
	memset(tmp,0,sizeof(char)*12);
	char*recvbuf = (char*)malloc(sizeof(char)*(pktsize + 8));
	memset(recvbuf, 0, sizeof(char)*(pktsize + 8));
	memcpy(tmp, &num, sizeof(num));
	memcpy(tmp + 4, &pktsize, sizeof(pktsize));
	memcpy(tmp + 8, &limitrate, sizeof(limitrate));

	if (send(sockfd, tmp, 12, 0) == SOCKET_ERROR){
		printf("send failed.Error code:%i\n", WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayC, interval);

	while (1){
		int buflen = recv(sockfd, recvbuf, pktsize + 8, 0);
		if (buflen == SOCKET_ERROR){
			printf("send failed.Error code:%i\n", WSAGetLastError());
			m.lock();
			flag = 1;
			m.unlock();
			th.join();
			closesocket(sockfd);
			return -1;
		}

		if (buflen == 0)
		{

			break;

		}
		memcpy(&num, recvbuf, 4);
		memcpy(&seq, recvbuf + 4, 4);
		len = len + buflen;

		m.lock();
		pkts = tmpkts + ((unsigned long int)len / (pktsize + 8));
		duration = (double)(es1.Elapsed()) / 1000;
		rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
		m.unlock();


		if (len >= 400000000)
		{
			tmpkts = pkts;
			len = len % (pktsize + 8);
		}
	}
	m.lock();
	flag = 1;
	m.unlock();
	th.join();
	closesocket(sockfd);
	return 0;
	
}

int TCP_Server(char*Port)
{
	SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
		printf("Socket Error!\n");
		printf("terminated\n");
		exit(-1);  //error
	}
	struct sockaddr_in MyAddr, peerAddr;
	MyAddr.sin_family = AF_INET;
	MyAddr.sin_addr.s_addr = INADDR_ANY;
	MyAddr.sin_port = htons(atoi(Port));

	if (bind(sockfd, (struct sockaddr *)&MyAddr, sizeof(MyAddr)) == SOCKET_ERROR){
		printf("TCP bind failed.Error code:%i\n", WSAGetLastError());
		closesocket(sockfd);
		printf("terminated\n");
		exit(-1);
	}

	listen(sockfd, 5);
	socklen_t len = sizeof(peerAddr);
	while (1){
		SOCKET newsockfd = accept(sockfd, (struct sockaddr *)&peerAddr, &len);
		o.lock();
		jobnumber++;
		std::thread th(TCP_thread, newsockfd, jobnumber);
		th.detach();
		o.unlock();
		m.lock();
		countTCP++;
		m.unlock();
	}	
}

int UDP_Client(char*Interval,char*Host_IP,char*Port,char*Pktsize,char*Rate,char*Num)
{
	int num, seq, pktsize;
	int i = 0;
	int interval = atoi(Interval);
	int limitrate = atoi(Rate);
	pkts = 0;
	pktsize=atoi(Pktsize);
	num=atoi(Num);
	socklen_t len = sizeof(sockaddr_in);

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

	char*tmp = (char*)malloc(sizeof(char)* 12);
	memset(tmp, 0, sizeof(char)* 12);
	char*recvbuf = (char*)malloc(sizeof(char)*(pktsize + 8));
	memset(recvbuf, 0, sizeof(char)*(pktsize + 8));
	memcpy(tmp, &num, sizeof(num));
	memcpy(tmp + 4, &pktsize, sizeof(pktsize));
	memcpy(tmp + 8, &limitrate, sizeof(limitrate));

	if (sendto(sockfd, tmp, 12, 0, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
		printf("send failed.Error code:%i\n", WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}

	ES_FlashTimer es1 = ES_FlashTimer();
	es1.Start();
	std::thread th(displayC, interval);

	while (1){
		int buflen = recvfrom(sockfd, recvbuf, pktsize + 8, 0, (struct sockaddr*)&peerAddr, &len);
		if (buflen == SOCKET_ERROR){
			printf("send failed.Error code:%i\n", WSAGetLastError());
			m.lock();
			flag = 1;
			m.unlock();
			th.join();
			closesocket(sockfd);
			return -1;
		}
		memcpy(&num, recvbuf, 4);
		memcpy(&seq, recvbuf + 4, 4);

		i++;
		m.lock();
		pkts = i;
		duration = (double)(es1.Elapsed()) / 1000;
		rate = ((long double)pkts*pktsize) / (duration * 1024 * 1024);
		lostrate = ((seq - pkts) / (double)seq) * 100;
		lostnum = seq - pkts;
		m.unlock();

		if (seq == num){
			break;
		}
	}
	m.lock();
	flag = 1;
	m.unlock();
	th.join();
	closesocket(sockfd);
	return 0;
}

int UDP_Server(char*Port)
{
	socklen_t len = sizeof(sockaddr_in);

	SOCKET sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sockfd == -1) {
		printf("Socket Error!\n");
		printf("terminated\n");
		exit(-1);  //error
	}
	struct sockaddr_in MyAddr, peerAddr;
	MyAddr.sin_family = AF_INET;
	MyAddr.sin_addr.s_addr = INADDR_ANY;
	MyAddr.sin_port = htons(atoi(Port));

	if (bind(sockfd, (struct sockaddr *)&MyAddr, sizeof(MyAddr)) == SOCKET_ERROR){
		printf("UDP bind failed.Error code:%i\n", WSAGetLastError());
		closesocket(sockfd);
		printf("terminated\n");
		exit(-1);
	}

	while (1){
		char*tmp = (char*)malloc(sizeof(char)* 12);
		memset(tmp, 0, sizeof(char)* 12);
		if (recvfrom(sockfd, tmp, 12, 0, (struct sockaddr*)&peerAddr, &len) == SOCKET_ERROR){
			printf("UDP request receiving failed.Error code:%i\n", WSAGetLastError());
			closesocket(sockfd);
			printf("terminated\n");
			exit(-1);
		}
		o.lock();
		jobnumber++;
		std::thread th(UDP_thread, peerAddr,tmp,jobnumber);
		th.detach();
		o.unlock();
		n.lock();
		countUDP++;
		n.unlock();
	}
	
}


int main(int argc,char*argv[])
{
#ifdef WIN32
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
#endif
	// Setup Winsock communication code here 
	if(argc!=3&&argc!=5&&argc!=9){
		PrintUsage();
		exit(-1);
	}
	if((strcmp(argv[1],"c")==0)&&(strcmp(argv[5],"tcp")==0)){
		if(TCP_Client(argv[2],argv[3],argv[4],argv[6],argv[7],argv[8])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if((strcmp(argv[1],"c")==0)&&(strcmp(argv[5],"udp")==0)){
		if(UDP_Client(argv[2],argv[3],argv[4],argv[6],argv[7],argv[8])==-1){
			printf("terminated\n");
			exit(-1);
		}
	}
	else if(strcmp(argv[1],"s")==0){
		std::thread th(TCP_Server, argv[3]);
		std::thread th1(UDP_Server,argv[4]);
		std::thread th2(displayS, argv[2]);
		
		th.join();
		th1.join();
		th2.join();
	}
	
	else{
		PrintUsage();
		exit(-1);
	}
	// When your application is finished call WSACleanup
#ifdef WIN32
	if (WSACleanup() == SOCKET_ERROR)
	{
		printf("WSACleanup failed with error %d\n", WSAGetLastError());
	}
#endif
	return 0;
}
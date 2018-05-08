#ifdef WIN32 //Windows
#include <winsock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <io.h>
#include <time.h>
#include <fcntl.h>
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
#include <condition_variable>
#include <queue>
#define VERSION "1.3.27"
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404


std::mutex mtx; //block thread
std::mutex lckq; //lock queue
std::queue<SOCKET> q1;
std::condition_variable cv;
bool flag = false;


void PrintUsage(){
	printf("invalid command line\n");
	printf("Usage:-------------------------------------------------------------------------------------------\n");
	printf("      NetProbe s [server_port] [threadmode] [threadnum]\n");
	printf("      threadmode: 'o' means on-demand thread creation mode\n");
	printf("      threadmode: 'p' means thread-pool model\n");
	printf("      threadnum: the number of threads only for thread-pool mode\n");
	printf("      -------------------------------------------------------------------------------------------\n");
}


int OnDemand_thread(SOCKET newsockfd){
	static char buffer[BUFSIZE+1];
	int ret,len,i;
	char * fstr;
	int file_fd;
	fstr = "text/html";
	ret = recv(newsockfd, buffer, BUFSIZE + 1, 0);
	//printf("%s\n", buffer);
	if (ret == SOCKET_ERROR){
		printf("TCP request receiving failed.Error code:%i\n", WSAGetLastError());
		closesocket(newsockfd);
		return -1;
	}
	for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
		if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = '\0';
			break;
		}
	} 
	if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) /* convert no filename to index file */
		strcpy(buffer, "GET /index.html");
	//printf("%s\n", &buffer[5]);
	if ((file_fd =open(&buffer[5],O_RDONLY)) == -1){  /* open the file for reading */
		printf("No such a file \n");
		closesocket(newsockfd);
		return 0;
	}
	
	len = lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
	sprintf(buffer, "HTTP/1.1 200 OK\nServer: lqk/%s\nContent-Length: %d\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
	send(newsockfd, buffer, strlen(buffer),0);

	/* send file in 8KB block - last block may be smaller */
	while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
		send(newsockfd, buffer, ret, 0);
	}
	close(file_fd);
	closesocket(newsockfd);
	return 0;
}

int OnDemand_Server(char*Port)
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
		//printf("%d\n",newsockfd);
		std::thread th(OnDemand_thread, newsockfd);
		th.detach();
		Sleep(2);
	}	
}

int ThreadPool_thread(void){
	SOCKET newsockfd;
	static char buffer[BUFSIZE + 1];
	int ret, len, i;
	char * fstr;
	int file_fd;
	fstr = "text/html";
	while(1) {
		std::unique_lock <std::mutex> lck(mtx);
		while (q1.empty()){
			cv.wait(lck);
		}
		lckq.lock();
		newsockfd = q1.front();
		q1.pop();
		lckq.unlock();

		/* Get to work */
		ret = recv(newsockfd, buffer, BUFSIZE + 1, 0);
		//printf("%s\n", buffer);
		if (ret == SOCKET_ERROR){
			printf("TCP request receiving failed.Error code:%i\n", WSAGetLastError());
			closesocket(newsockfd);
			return -1;
		}
		for (i = 4; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
			if (buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
				buffer[i] = '\0';
				break;
			}
		}
		if (!strncmp(&buffer[0], "GET /\0", 6) || !strncmp(&buffer[0], "get /\0", 6)) /* convert no filename to index file */
			strcpy(buffer, "GET /index.html");
		//printf("%s\n",buffer);
		if ((file_fd = open(&buffer[5], O_RDONLY)) == -1){  /* open the file for reading */
			printf("No such a file\n");
			closesocket(newsockfd);
			continue;
		}

		len = lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
		lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
		sprintf(buffer, "HTTP/1.1 200 OK\nServer: lqk/%s\nContent-Length: %d\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
		send(newsockfd, buffer, strlen(buffer), 0);

		/* send file in 8KB block - last block may be smaller */
		while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
			send(newsockfd, buffer, ret, 0);
		}
		close(file_fd);
		closesocket(newsockfd);
	}
}

int ThreadPool_Server(char*Port,char*Threadnum)
{
	int i;
	int threadnum = atoi(Threadnum);
	std::thread *threads = (std::thread*) malloc(sizeof(std::thread)*threadnum);
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

	memset(threads, 0, sizeof(std::thread)*threadnum);
	for (i = 0; i < threadnum; i++) {
		threads[i] = std::thread(ThreadPool_thread);
		threads[i].detach();
		printf("creating thread %d\n", i + 1);
	}

	while (1){
		//std::unique_lock <std::mutex> lck(mtx);
		SOCKET newsockfd = accept(sockfd, (struct sockaddr *)&peerAddr, &len);
		//printf("%d\n",newsockfd);
		lckq.lock();
		q1.push(newsockfd);
		lckq.unlock();
		cv.notify_one();
		Sleep(2);
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
	if (argc != 4 && argc != 5 && argc != 9){
		PrintUsage();
		exit(-1);
	}
	
	else if(strcmp(argv[1],"s")==0){
		if (strcmp(argv[3], "o") == 0){
			OnDemand_Server(argv[2]);
		}
		if (strcmp(argv[3], "p") == 0){
			ThreadPool_Server(argv[2],argv[4]);	
		}
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
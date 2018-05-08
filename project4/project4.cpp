#ifdef WIN32 //Windows
#include <winsock2.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <direct.h>
#include <stdio.h>
#include <fcntl.h>
#include <atlstr.h>
#include <sys/utime.h>
#include <io.h>
#pragma comment(lib,"Ws2_32.lib") 
#else  //linux
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <utime.h>
#include <dirent.h>

#define SOCKET int
#define SOCKET_ERROR -1
#define WSAGetLastError() (errno)
#define closesocket(s) close(s)
#define Sleep(s) usleep(s*1000)
#define ioctlsocket ioctl
#define WSAWOULDBLOCK EWOULDBLOCK
#define _mkdir(s) mkdir(s,00700)
#endif
#include<sys/stat.h>
#include<sys/types.h>
#include <thread>
#include <mutex>
#include <list>
#include <iostream>
#include <time.h>
#define VERSION "1.3.27"
#define BUFSIZE 8096
#define FORBIDDEN 403
#define NOTFOUND  404
#define Port 4160

std::list<sockaddr_in> list;
std::mutex mtx;   //for file read write
std::mutex mtx1;  //for cookie number
int cookienum = 0;
#ifdef WIN32
DWORD cbBytes=0;
char file_name[MAX_PATH]; 
char file_name2[MAX_PATH]; 
char notify[32768];
#endif

void PrintUsage(){
	printf("invalid command line\n");
	printf("Usage:-------------------------------------------------------------------------------------------\n");
	printf("      NetProbe s [server_dir_name]\n");
	printf("      NetProbe c [server ip] [client_dir_name]\n");
	printf("      -------------------------------------------------------------------------------------------\n");
}

#ifdef WIN32
void fileWatcher(char*Dir,char*Host_IP)
{
	char buffer[BUFSIZE + 1];
	cbBytes=0; 
	memset(file_name,0,MAX_PATH); //设置文件名
	memset(file_name2,0,MAX_PATH); //设置文件重命名后的名字
	memset(notify,0,32768);           
	USES_CONVERSION;
	TCHAR *dir=A2T(Dir);

	HANDLE dirHandle = CreateFile(dir,GENERIC_READ | GENERIC_WRITE | FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);

	if(dirHandle == INVALID_HANDLE_VALUE) 
	{
		printf("monitor failed.Error code:%i\n", WSAGetLastError());
		exit(-1);
	}


	memset(notify,0,strlen(notify));

	FILE_NOTIFY_INFORMATION *pnotify = (FILE_NOTIFY_INFORMATION*)notify; 

	while (1){
		if (ReadDirectoryChangesW(dirHandle, pnotify, 32768, true,
			FILE_NOTIFY_CHANGE_FILE_NAME
			| FILE_NOTIFY_CHANGE_DIR_NAME
			| FILE_NOTIFY_CHANGE_CREATION
			| FILE_NOTIFY_CHANGE_LAST_WRITE
			| FILE_NOTIFY_CHANGE_SIZE,
			&cbBytes, NULL, NULL))
		{
			//转换文件名为多字节字符串
			if (pnotify->FileName)
			{
				memset(file_name, 0, strlen(file_name));

				WideCharToMultiByte(CP_ACP, 0, pnotify->FileName, pnotify->FileNameLength / 2, file_name, 99, NULL, NULL);
			}

			//获取重命名的文件名
			if (pnotify->NextEntryOffset != 0 && (pnotify->FileNameLength > 0 && pnotify->FileNameLength < MAX_PATH))
			{
				PFILE_NOTIFY_INFORMATION p = (PFILE_NOTIFY_INFORMATION)((char*)pnotify + pnotify->NextEntryOffset);
				memset(file_name2, 0, sizeof(file_name2));
				WideCharToMultiByte(CP_ACP, 0, p->FileName, p->FileNameLength / 2, file_name2, 99, NULL, NULL);
			}

			if (pnotify->Action == FILE_ACTION_REMOVED )                //设置类型过滤器:删除
			{
				SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				memset(buffer, 0, BUFSIZE + 1);

				if (sockfd == -1) {
					printf("Socket Error!\n");
					//break;  //error
				}
				struct sockaddr_in peerAddr;
				peerAddr.sin_family = AF_INET;
				peerAddr.sin_addr.s_addr = inet_addr(Host_IP);
				peerAddr.sin_port = htons(Port);
				if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
					printf("connect() failed.Error code:%i\n", WSAGetLastError());
					closesocket(sockfd);
					//break;
				}

				sprintf(buffer, "Remove /%s", file_name);
				send(sockfd, buffer, strlen(buffer), 0);
				closesocket(sockfd);
				continue;
			}

			if (pnotify->Action == FILE_ACTION_RENAMED_OLD_NAME)                //设置类型过滤器:重命名
			{
				SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				memset(buffer, 0, BUFSIZE + 1);

				if (sockfd == -1) {
					printf("Socket Error!\n");
					//break;  //error
				}
				struct sockaddr_in peerAddr;
				peerAddr.sin_family = AF_INET;
				peerAddr.sin_addr.s_addr = inet_addr(Host_IP);
				peerAddr.sin_port = htons(Port);
				if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
					printf("connect() failed.Error code:%i\n", WSAGetLastError());
					closesocket(sockfd);
					//break;
				}

				sprintf(buffer, "Remove /%s", file_name);
				send(sockfd, buffer, strlen(buffer), 0);
				closesocket(sockfd);
			}
		}
		break;
	}
	CloseHandle(dirHandle);
}

int Client_thread1(SOCKET Newsockfd,char* Str)
{
	SOCKET newsockfd=Newsockfd;
	char str[256];
	int ret=0;
		int i,j,k;
		int clen,time;
		int rlen=0;
		int file_fd=0;
		char tmp[12];

		char buffer[BUFSIZE+1];
		char recvbuffer[BUFSIZE + 1];
		memset(buffer, 0, BUFSIZE + 1);
		memset(recvbuffer, 0, BUFSIZE + 1);
		memset(tmp,0,12);
		strcpy(str, Str);
		ret = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
		if (ret == SOCKET_ERROR){
			printf("TCP request receiving failed.Error code:%i\n", WSAGetLastError());
			closesocket(newsockfd);
			return -1;
		}
		if(recvbuffer[0]=='R'){                                   //Remove file
			strcpy(str + strlen(str), &recvbuffer[7]);
			remove(str);
			printf("file %s removed or renamed\n",str);
		}

		else
		{
			for (i = 5; i < BUFSIZE; i++) { 	
				if(recvbuffer[i]==' '&&strcspn(recvbuffer,".")<i){
					recvbuffer[i] = '\0';
					break;
				}
			}
			for (i = 5; i < BUFSIZE; i++) {
				if (recvbuffer[i] == '\n')   // get second line;
				{
					j = i;
					break;
				}
			}
			i = i + 1;
			for (i; i < BUFSIZE; i++){
				if (recvbuffer[i] == '\n')
				{
					k = i;
					break;
				}
			}
			memcpy(tmp, recvbuffer + j + 17, k - (j + 17));         //get Content-length len
			clen = atoi(tmp);
			memset(tmp,0,12);
			i = i + 1;
			for (i; i < BUFSIZE; i++){
				if (recvbuffer[i] == '\n')
				{
					break;
				}
			}
			memcpy(tmp, recvbuffer + (k + 13), i - (k + 13));         //get Last-mtime time
			time = atoi(tmp);
			strcpy(str + strlen(str), &recvbuffer[5]);
			sprintf(buffer, "OK\n");
			send(newsockfd, buffer, strlen(buffer), 0);
			mtx.lock();
			if (access(str, 0) == -1){  /* open the file for reading */
				struct utimbuf new_times;
				struct stat attrib;
				printf("No such a file %s\n",str);
				file_fd = open(str, O_CREAT | O_RDWR|O_BINARY, 00700);
				printf("file %s created\n", str);
				memset(recvbuffer, 0, BUFSIZE + 1);
				while (rlen < clen){
					int x;
					x = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
					rlen = rlen + write(file_fd, recvbuffer, x);
					//printf("%d\n", errno);
					memset(recvbuffer, 0, BUFSIZE + 1);
				};
				close(file_fd);
				stat(str, &attrib);
				new_times.actime = attrib.st_atime; /* keep atime unchanged */
				new_times.modtime = time;           //change mtime to the client's mtime
				utime(str, &new_times);
			}
			else{
				struct stat attrib;
				stat(str, &attrib);
				//printf("%d,%d\n", time,attrib.st_mtime);
				if (time > attrib.st_mtime)
				{
					struct utimbuf new_times;
					file_fd = open(str, O_RDWR|O_TRUNC|O_BINARY);
					memset(recvbuffer, 0, BUFSIZE + 1);
					while (rlen < clen){
						int x;
						x = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
						rlen = rlen + write(file_fd, recvbuffer, x);
						memset(recvbuffer, 0, BUFSIZE + 1);
					}
					close(file_fd);
					new_times.actime = attrib.st_atime; 
					new_times.modtime = time;           //change mtime to the client's mtime
					utime(str, &new_times);
					printf("file %s renewed\n", str);
				}
			}
			mtx.unlock();
		}
		closesocket(newsockfd);
	
	return 0;
}

int client_thread(char* Str,sockaddr_in Clientthread)
{
	SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
		printf("Socket Error!\n");
		printf("terminated\n");
		exit(-1);  //error
	}
	struct sockaddr_in MyAddr, peerAddr;
	MyAddr=Clientthread;


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
		std::thread th(Client_thread1, newsockfd,Str);
		th.detach();

	}

	return 0;
}

int TCP_Client(char*Host_IP, char*Str)
{
	int i = 0;
	char buffer[BUFSIZE + 1];
	char recvbuffer[BUFSIZE + 1];
	int j,k,l;
	int clen = 0;
	int wlen = 0;
	int file_fd=0;
	int ret = 0;
	int cookie;
	int time;
	char tmp[12];
	char str[256];
	sockaddr_in Clientthread;
	int len;
	len=sizeof(Clientthread);

	for (i = 0; i < strlen(Str); i++)
	{
		if (Str[i] == '\\')
		{
			Str[i] = '\0';
			break;
		}
	}
	if (_mkdir(Str) == -1)
	{
		if (errno != 17)
		{
			printf("cannot create directory\n");
			return 0;
		}
	}
	strcpy(Str + strlen(Str), "/cloud");
	if (_mkdir(Str) == -1)
	{
		if (errno == 17)
		{
			printf("Directory already there\n");
		}
		else{
			printf("cannot create directory\n");
			return 0;
		}
	}
	else printf("Directory created successfully\n");


	SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);                               //download files from server when connection instablished
	memset(buffer, 0, BUFSIZE + 1);
	memset(recvbuffer, 0, BUFSIZE + 1);

	if (sockfd == -1) {
		printf("Socket Error!\n");
		return -1;  //error
	}
	struct sockaddr_in peerAddr;
	peerAddr.sin_family = AF_INET;
	peerAddr.sin_addr.s_addr = inet_addr(Host_IP);
	peerAddr.sin_port = htons(Port);
	if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
		printf("connect() failed.Error code:%i\n", WSAGetLastError());
		closesocket(sockfd);
		return -1;
	}

	sprintf(buffer, "NAME\n");
	send(sockfd, buffer, strlen(buffer), 0);
	while (1)
	{
		strcpy(str, Str);
		memset(buffer, 0, BUFSIZE + 1);
		memset(recvbuffer, 0, BUFSIZE + 1);
		memset(tmp, 0, 12);
		wlen = 0;
		recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
		if (recvbuffer[0] == 'O')
		{
			for (i = 0; i < BUFSIZE; i++) {
				if (recvbuffer[i] == '\n')   // get second line;
				{
					j = i;
					break;
				}
			}
			i = i + 1;
			for (i; i < BUFSIZE; i++){
				if (recvbuffer[i] == '\n')
				{
					break;
				}
			}
			memcpy(tmp, recvbuffer + (j+ 13), i - (j + 13));         //get Set-Cookie: %d
			cookie = atoi(tmp);
			printf("My cookie is %d\n", cookie);
			break;
		}
		else
		{
			for (i = 5; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
				if (recvbuffer[i] == ' '&&strcspn(recvbuffer, ".") < i) { /* string is "GET URL " +lots of other stuff */
					recvbuffer[i] = '\0';
					break;
				}
			}
			for (i = 5; i < BUFSIZE; i++) {
				if (recvbuffer[i] == '\n')   // get second line;
				{
					j = i;
					break;
				}
			}
			i = i + 1;
			for (i; i < BUFSIZE; i++){
				if (recvbuffer[i] == '\n')
				{
					k = i;
					break;
				}
			}
			memcpy(tmp, recvbuffer + j + 17, k - (j + 17));         //get content-length len
			clen = atoi(tmp);
			memset(tmp, 0, 12);
			i = i + 1;
			for (i; i < BUFSIZE; i++){
				if (recvbuffer[i] == '\n')
				{
					l = i;
					break;
				}
			}
			memcpy(tmp, recvbuffer + (k + 13), i - (k + 13));         //get Last-mtime time
			time = atoi(tmp);
			strcpy(str + strlen(str), &recvbuffer[5]);
				if (access(str, 0) == -1){  /* open the file for reading */
					struct utimbuf new_times;
					struct stat attrib;
					printf("No such a file %s\n", str);
					sprintf(buffer, "No\n"); /* Header + a blank line */
					send(sockfd, buffer, strlen(buffer), 0);
					file_fd = open(str, O_CREAT | O_RDWR | O_BINARY, 00700);
					printf("file %s created\n", str);

					memset(recvbuffer, 0, BUFSIZE + 1);
					while (wlen < clen){
						int x;
						x = recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
						wlen = wlen + write(file_fd, recvbuffer, x);
						memset(recvbuffer, 0, BUFSIZE + 1);
					}
					close(file_fd);
					stat(str, &attrib);
					new_times.actime = attrib.st_atime; /* keep atime unchanged */
					new_times.modtime = time;           //change mtime to the server's mtime
					utime(str, &new_times);
					memset(buffer, 0, BUFSIZE + 1);
					sprintf(buffer, "OK\n");
					send(sockfd, buffer, strlen(buffer), 0);
				}
				else {
					sprintf(buffer, "Yes\n");
					send(sockfd, buffer, strlen(buffer), 0);
				}
			}

		}
		getsockname(sockfd, (sockaddr*)&Clientthread, &len);
		closesocket(sockfd);

		std::thread th(client_thread, Str, Clientthread);
		th.detach();


		while (1){
			int handle = 0;
			struct _finddata_t  c_file;
			j = 0;
			k = 0;
			clen = 0;
			wlen = 0;
			file_fd = 0;
			ret = 0;
			time = 0;
			strcpy(str, Str);

			strcpy(str + strlen(str), "/*.*");

			mtx.lock();
			handle = _findfirst(str, &c_file);
			_findnext(handle, &c_file);
			while (_findnext(handle, &c_file) == 0)
			{
				SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				memset(buffer, 0, BUFSIZE + 1);
				memset(recvbuffer, 0, BUFSIZE + 1);
				memset(tmp, 0, 12);
				strcpy(str, Str);

				if (sockfd == -1) {
					printf("Socket Error!\n");
					return -1;  //error
				}
				struct sockaddr_in peerAddr;
				peerAddr.sin_family = AF_INET;
				peerAddr.sin_addr.s_addr = inet_addr(Host_IP);
				peerAddr.sin_port = htons(Port);
				if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
					printf("connect() failed.Error code:%i\n", WSAGetLastError());
					closesocket(sockfd);
					return -1;
				}
				sprintf(buffer, "POST /%s HTTP/1.1\nFile-length: %d\nLast-mtime: %d\n", c_file.name, c_file.size, c_file.time_write); /* Header + a blank line */
				sprintf(buffer + strlen(buffer), "Cookie: %d\n\n", cookie);
				send(sockfd, buffer, strlen(buffer), 0);
				for (i = 5; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
					if (buffer[i] == ' '&&strcspn(buffer, ".") < i) { /* string is "GET URL " +lots of other stuff */
						buffer[i] = '\0';
						break;
					}
				}
				strcpy(str + strlen(str), &buffer[5]);
				memset(buffer, 0, BUFSIZE + 1);
				recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
				for (i = 0; i < BUFSIZE; i++){
					if (recvbuffer[i] == '\n')
					{
						j = i;
						break;
					}
				}
				i = i + 1;
				for (i; i < BUFSIZE; i++){
					if (recvbuffer[i] == '\n')
					{
						k = i;
						break;
					}
				}
				memcpy(tmp, recvbuffer + (j + 17), k - (j + 17));         //get Content-Length: x
				clen = atoi(tmp);
				memset(tmp, 0, 12);
				i = i + 1;
				for (i; i < BUFSIZE; i++){
					if (recvbuffer[i] == '\n')
					{
						break;
					}
				}
				memcpy(tmp, recvbuffer + (k + 13), i - (k + 13));         //get Last-mtime: x
				time = atoi(tmp);
				if (c_file.time_write>time)
				{
					printf("uploading file %s to server\n", str);
					file_fd = open(str, O_RDONLY | O_BINARY);
					//printf("%d\n",errno);
					while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
						send(sockfd, buffer, ret, 0);
						memset(buffer, 0, BUFSIZE + 1);
					}
					close(file_fd);

				}
				else if (c_file.time_write < time)
				{
					struct utimbuf new_times;
					struct stat attrib;
					printf("downloading file %s from server\n", str);
					file_fd = open(str, O_RDWR | O_TRUNC | O_BINARY);
					memset(recvbuffer, 0, BUFSIZE + 1);
					while (wlen < clen){
						int x;
						x = recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
						wlen = wlen + write(file_fd, recvbuffer, x);
						memset(recvbuffer, 0, BUFSIZE + 1);
					}
					close(file_fd);
					stat(str, &attrib);
					new_times.actime = attrib.st_atime; /* keep atime unchanged */
					new_times.modtime = time;           //change mtime to the server's mtime
					utime(str, &new_times);
				}
				closesocket(sockfd);
			}
			mtx.unlock();
			_findclose(handle);
			fileWatcher(Str, Host_IP);                               //monitor changes
			Sleep(200);
		}


	return 0;

}
#endif

int OnDemand_thread(SOCKET newsockfd, char*Str, sockaddr_in PeerAddr){
	char buffer[BUFSIZE+1];
	char recvbuffer[BUFSIZE + 1];
	char tmp[12];
	char Filelen[10];
	char str[256];
	int time;
	int ret,len,i,j,k,l;
	int rlen = 0;
	int file_fd;
	int id;
	int cookie;
	struct sockaddr_in thispeerAddr=PeerAddr;
	memset(buffer, 0, BUFSIZE + 1);
	memset(recvbuffer, 0, BUFSIZE + 1);
	memset(tmp,0,12);

	strcpy(str, Str);
	ret = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
	//printf("%s\n", buffer);
	if (ret == SOCKET_ERROR){
		printf("TCP request receiving failed.Error code:%i\n", WSAGetLastError());
		closesocket(newsockfd);
		return -1;
	}

#ifdef WIN32
	if(recvbuffer[0]=='N')                                          //sending files to the client at very beginning             
	{
		int handle=0;
		struct _finddata_t  c_file;
		mtx1.lock();
		memcpy(PeerAddr.sin_zero, &cookienum, 4);
		list.push_back(PeerAddr);
		strcpy(str + strlen(str), "/*.*");
		handle = _findfirst(str, &c_file);
		_findnext(handle, &c_file);

		while (_findnext(handle, &c_file) == 0)
		{
			memset(buffer, 0, BUFSIZE + 1);
			sprintf(buffer, "NAME /%s \nContent-Length: %d\nLast-mtime: %d\n\n", c_file.name, c_file.size, c_file.time_write); 
			send(newsockfd, buffer, strlen(buffer), 0);
			memset(recvbuffer,0,BUFSIZE+1);
			recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
			if(recvbuffer[0]=='N')
			{
				strcpy(str, Str);
				sprintf(str + strlen(str), "/%s",c_file.name);
				mtx.lock();
				file_fd = open(str, O_RDWR);
				memset(buffer, 0, BUFSIZE + 1);
				/* send file in 8KB block - last block may be smaller */
				while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
					send(newsockfd, buffer, ret, 0);
					memset(buffer, 0, BUFSIZE + 1);
				}
				close(file_fd);
				mtx.unlock();
				memset(recvbuffer,0,BUFSIZE+1);
				recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
			}		
		}
		memset(buffer, 0, BUFSIZE + 1);
		sprintf(buffer, "OVER\nSet-Cookie: %d\n\n",cookienum); 
		id = cookienum;
		cookienum++;
		mtx1.unlock();
		send(newsockfd, buffer, strlen(buffer), 0);
	}
#else
	if (recvbuffer[0] == 'N')                                          //sending files to the client at very beginning             
	{
		int handle = 0;
		DIR*dp;
		struct dirent*dirp;
		mtx1.lock();
		memcpy(PeerAddr.sin_zero, &cookienum, 4);
		list.push_back(PeerAddr);
		if ((dp = opendir(str)) == NULL)
			printf("cannot open %s", str);
		readdir(dp);
		readdir(dp);
		while ((dirp = readdir(dp)) != NULL)
		{
			int file_fd;
			struct stat attrib;
			stat(dirp->d_name,&attrib);
			memset(buffer, 0, BUFSIZE + 1);
			sprintf(buffer, "NAME /%s \nContent-Length: %d\nLast-mtime: %d\n\n", dirp->d_name,attrib.st_size, attrib.st_mtime);
			printf("%s,%d\n",dirp->d_name,attrib.st_size);
			send(newsockfd, buffer, strlen(buffer), 0);
			memset(recvbuffer, 0, BUFSIZE + 1);
			recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
			if (recvbuffer[0] == 'N')
			{
				strcpy(str, Str);
				sprintf(str + strlen(str), "/%s", dirp->d_name);
				mtx.lock();
				file_fd = open(str, O_RDWR);
				memset(buffer, 0, BUFSIZE + 1);
				/* send file in 8KB block - last block may be smaller */
				while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
					send(newsockfd, buffer, ret, 0);
					memset(buffer, 0, BUFSIZE + 1);
				}
				close(file_fd);
				mtx.unlock();
				memset(recvbuffer, 0, BUFSIZE + 1);
				recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
			}
		}
		memset(buffer, 0, BUFSIZE + 1);
		sprintf(buffer, "OVER\nSet-Cookie: %d\n\n", cookienum);
		id=cookienum;
		cookienum++;
		mtx1.unlock();
		send(newsockfd, buffer, strlen(buffer), 0);
	}
#endif

	else if(recvbuffer[0]=='R'){                                   //Remove file
		strcpy(str + strlen(str), &recvbuffer[7]);
		if(access(str, 0) == 0)
		{
			mtx.lock();
			remove(str);
			mtx.unlock();
			printf("file %s removed or renamed\n",str);
			std::list<sockaddr_in>::iterator it;  
			for(it=list.begin();it!=list.end();it++)  
			{  
				sockaddr_in peerAddr = *it;  

				SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				memset(buffer, 0, BUFSIZE + 1);

				if (sockfd == -1) {
					printf("Socket Error!\n");
					return -1;  //error
				}

				if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
				//	printf("connect() failed.Error code:%i\n", WSAGetLastError());
					closesocket(sockfd);
					continue;
				}
				sprintf(buffer, "Remove /%s", &recvbuffer[8]);
				send(sockfd, buffer, strlen(buffer), 0);  
				closesocket(sockfd);

			}  
		}
	}

	else{

		for (i = 5; i < BUFSIZE; i++) { /* null terminate after the second space to ignore extra stuff */
			if (recvbuffer[i] == ' '&&strcspn(recvbuffer,".")<i) { /* string is "GET URL " +lots of other stuff */
				recvbuffer[i] = '\0';
				break;
			}
		}
		for (i = 5; i < BUFSIZE; i++) {
			if (recvbuffer[i] == '\n')   // get second line;
			{
				j = i;
				break;
			}
		}
		i = i + 1;
		for (i; i < BUFSIZE; i++){
			if (recvbuffer[i] == '\n')
			{
				k = i;
				break;
			}
		}
		memcpy(tmp, recvbuffer + j + 14, k - (j + 14));         //get file-length len
		len = atoi(tmp);
		memset(tmp,0,12);
		i = i + 1;
		for (i; i < BUFSIZE; i++){
			if (recvbuffer[i] == '\n')
			{
				l = i;
				break;
			}
		}
		memcpy(tmp, recvbuffer + (k + 13), i - (k + 13));         //get Last-mtime time
		time = atoi(tmp);
		memset(tmp, 0, 12);
		i = i + 1;
		for (i; i < BUFSIZE; i++){
			if (recvbuffer[i] == '\n')
			{
				break;
			}
		}
		memcpy(tmp, recvbuffer + (l + 9), i - (l + 9));         //get Cookie: %d
		cookie = atoi(tmp);
		strcpy(str + strlen(str), &recvbuffer[5]);
		memset(recvbuffer, 0, BUFSIZE + 1);
		if (access(str, 0) == -1){  /* open the file for reading */
			struct utimbuf new_times;
			struct stat attrib;
			printf("No such a file %s\n",str);
			sprintf(buffer, "HTTP/1.1 200 OK\nContent-Length: 0\nLast-mtime: 0\n\n"); /* Header + a blank line */
			send(newsockfd, buffer, strlen(buffer), 0);
			mtx.lock();
			file_fd = open(str, O_CREAT | O_RDWR, 00700);
			printf("file %s created\n", str);
			while (rlen < len){
				int x;
				x = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
				rlen = rlen + write(file_fd, recvbuffer, x);
				//printf("%d\n", errno);
				memset(recvbuffer, 0, BUFSIZE + 1);
			}
			//fstat(file_fd, &attrib);
			close(file_fd);
			stat(str, &attrib);
			new_times.actime = attrib.st_atime; /* keep atime unchanged */
			new_times.modtime = time;           //change mtime to the client's mtime
			utime(str, &new_times);
			mtx.unlock();

			std::list<sockaddr_in>::iterator it;  
			mtx.lock();
			for(it=list.begin();it!=list.end();it++)  
			{  
				int file_fd1;
				sockaddr_in peerAddr = *it;  
				memcpy(&id, peerAddr.sin_zero, 4);
				if (id != cookie){
					SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
					memset(buffer, 0, BUFSIZE + 1);

					if (sockfd == -1) {
						printf("Socket Error!\n");
						return -1;  //error
					}

					if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
			//			printf("connect() failed.Error code:%i\n", WSAGetLastError());
						closesocket(sockfd);
						continue;
					}
					sprintf(buffer, "Send /%s \nContent-Length: %d\nLast-mtime: %d\n\n", str + strlen(Str) + 1, len, time);
					send(sockfd, buffer, strlen(buffer), 0);
					memset(buffer, 0, BUFSIZE + 1);
					recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
					file_fd1 = open(str, O_RDWR );
					while ((ret = read(file_fd1, buffer, BUFSIZE)) > 0) {
						send(sockfd, buffer, ret, 0);
						memset(buffer, 0, BUFSIZE + 1);
					}
					close(file_fd1);
					closesocket(sockfd);
				}
			} 
			mtx.unlock();
		}
		else{
			struct stat attrib;
			stat(str, &attrib);
			//printf("%d,%d\n", time,attrib.st_mtime);
			if (time > attrib.st_mtime)
			{
				struct utimbuf new_times;
				mtx.lock();
				file_fd = open(str, O_RDWR|O_TRUNC);
				sprintf(buffer, "HTTP/1.1 200 OK\nContent-Length: 0\nLast-mtime: %d\n\n", attrib.st_mtime); /* Header + a blank line */
				send(newsockfd, buffer, strlen(buffer), 0);
				while (rlen < len){
					int x;
					x = recv(newsockfd, recvbuffer, BUFSIZE + 1, 0);
					rlen = rlen + write(file_fd, recvbuffer, x);
					memset(recvbuffer, 0, BUFSIZE + 1);
				}
				close(file_fd);
				new_times.actime = attrib.st_atime; /* keep atime unchanged */
				new_times.modtime = time;           //change mtime to the client's mtime
				utime(str, &new_times);
				mtx.unlock();
				printf("file %s renewed\n", str);

				std::list<sockaddr_in>::iterator it;  
				mtx.lock();
				for(it=list.begin();it!=list.end();it++)  
				{
					int file_fd1;
					sockaddr_in peerAddr = *it;  
					memcpy(&id, peerAddr.sin_zero, 4);
					if (id != cookie){
						SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
						memset(buffer, 0, BUFSIZE + 1);

						if (sockfd == -1) {
							printf("Socket Error!\n");
							return -1;  //error
						}

						if (connect(sockfd, (struct sockaddr*)&peerAddr, sizeof(peerAddr)) == SOCKET_ERROR){
						//	printf("connect() failed.Error code:%i\n", WSAGetLastError());
							closesocket(sockfd);
							continue;
						}
						sprintf(buffer, "Send /%s \nContent-Length: %d\nLast-mtime: %d\n\n", str + strlen(Str) + 1, len, time);
						send(sockfd, buffer, strlen(buffer), 0);
						memset(buffer, 0, BUFSIZE + 1);
						recv(sockfd, recvbuffer, BUFSIZE + 1, 0);
						file_fd1 = open(str, O_RDWR );
						while ((ret = read(file_fd1, buffer, BUFSIZE)) > 0) {
							send(sockfd, buffer, ret, 0);
							memset(buffer, 0, BUFSIZE + 1);
						}
						close(file_fd1);
						closesocket(sockfd);
					}
				}
				mtx.unlock();

			}
			else if (time < attrib.st_mtime)
			{
				mtx.lock();
				file_fd = open(str, O_RDWR);
				sprintf(buffer, "HTTP/1.1 200 OK\nContent-Length: %d\nLast-mtime: %d\n\n",attrib.st_size, attrib.st_mtime); /* Header + a blank line */
				send(newsockfd, buffer, strlen(buffer), 0);
				memset(buffer, 0, BUFSIZE + 1);
				/* send file in 8KB block - last block may be smaller */
				while ((ret = read(file_fd, buffer, BUFSIZE)) > 0) {
					send(newsockfd, buffer, ret, 0);
					memset(buffer, 0, BUFSIZE + 1);
				}
				close(file_fd);
				mtx.unlock();
			}
			else{
				sprintf(buffer, "HTTP/1.1 200 OK\nContent-Length: 0\nLast-mtime: %d\n\n", attrib.st_mtime); /* Header + a blank line */
				send(newsockfd, buffer, strlen(buffer), 0);
				memset(buffer, 0, BUFSIZE + 1);
			}

		}
	}
	closesocket(newsockfd);
	return 0;
}

int OnDemand_Server(char*Str)
{
	int i = 0;
	int n = 0;
	for (i = 0; i < strlen(Str); i++)
	{
		if (Str[i] == '\\')
		{
			Str[i] = '\0';
			break;
		}
	}
	if (_mkdir(Str) == -1 )
	{
		if (errno != 17)
		{
			printf("cannot create directory\n");
			return 0;
		}
	}
	strcpy(Str+strlen(Str), "/cloud");
	if (_mkdir(Str) == -1 )
	{
		if (errno == 17)
		{
			printf("Directory already there\n");
		}
		else{
			printf("cannot create directory\n");
			return 0;
		}
	}
	else printf("Directory created successfully\n");

	SOCKET sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd == -1) {
		printf("Socket Error!\n");
		printf("terminated\n");
		exit(-1);  //error
	}
	struct sockaddr_in MyAddr, peerAddr;
	MyAddr.sin_family = AF_INET;
	MyAddr.sin_addr.s_addr = INADDR_ANY;
	MyAddr.sin_port = htons(Port);

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
		std::thread th(OnDemand_thread, newsockfd,Str,peerAddr);
		th.detach();
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
	if (argc != 4 && argc != 3){
		PrintUsage();
		exit(-1);
	}

	else if(strcmp(argv[1],"s")==0){
		OnDemand_Server(argv[2]);
	}
#ifdef WIN32
	else if (strcmp(argv[1], "c") == 0){
		TCP_Client(argv[2], argv[3]);
	}
#endif
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

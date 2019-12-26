#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unistd.h>
#include <netdb.h>
#include <string.h>

using namespace std;


//relay data
int relay_data(int from,int to){
	const static int BUFSIZE = 1024;
	char buf[BUFSIZE];
	printf("send from %d to %d\n",from,to);
	while(true){
		ssize_t received = recv(from,buf,BUFSIZE-1,0);
		if(received ==0 || received == -1){
			perror("received failed\n");
			return -1;
		}
		buf[received] = '\0';
		printf("%s\n",buf);
		

		ssize_t sent = send(to,buf,BUFSIZE-1,0);
		if(sent ==0 || sent == -1){
			perror("send failed\n");
			return -1;
		}
	}

}


//find buf
char * find_hostname(char* buf){
	char *hostname_first = strstr(buf,"Host: ")+6;
	if(hostname_first == NULL){
		printf("cant find hostname\n");
		return NULL;
	}

	char *hostname_last = strstr(hostname_first,"\r\n");
	if(hostname_last ==NULL){
		printf("cant find hostname\n");
		return NULL;
	}
	int hostname_len = (int)(hostname_last-hostname_first);
	char* hostname = (char *)malloc(hostname_len+1);
	memset(hostname,0,sizeof(hostname));
	memcpy(hostname,hostname_first,hostname_len);
	return hostname;
}


int proxying(int clientfd){

	//receive and relay
	const static int BUFSIZE = 1024;
	char buf[BUFSIZE];
	ssize_t received = recv(clientfd, buf, BUFSIZE-1,0);
	if(received == 0 || received == -1){
		perror("receive failed\n");
		return -1;
	}
	buf[received] = '\0';

	//find hostname
	char * hostname = find_hostname(buf);
	if(hostname==NULL){
		return -1;
	}
	printf("host name : %s\n",hostname);


	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints,0x00, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int status = getaddrinfo(hostname, "80",&hints, &servinfo);
	free(hostname); //malloc free
	if(status !=0){
		perror("getaddrinfo error!\n");
		return -1;
	}

	int ip = 0;
	for(int i=0;i<4;i++){
		ip = ip<<8;
		ip += (int)servinfo->ai_addr->sa_data[i+2];
	}
	ip += 0x01000000;
	

	// create server socket
	int serverfd = socket(AF_INET, SOCK_STREAM,0);
	if(serverfd==-1){
		perror("server create socket failed\n");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(80);
	addr.sin_addr.s_addr = htonl(ip);
	memset(addr.sin_zero,0,sizeof(addr.sin_zero));

	int res = connect(serverfd, reinterpret_cast<struct sockaddr*>(&addr),sizeof(struct sockaddr));
	if(res==-1){
		perror("connect failed\n");
		return -1;
	}
	ssize_t sent = send(serverfd, buf,BUFSIZE-1,0);
		if(sent ==0){
			perror("send failed\n");
			return -1;
		}
		thread t1(&relay_data,clientfd,serverfd);
		thread t2(&relay_data,serverfd,clientfd);
		t1.join();
		t2.join();
}


void usage(){
	printf("syntax : web_proxy <tcp port> <ssl port>\n");
	printf("sample : web_proxy 8080 4433\n");
}

int main(int argc, char *argv[]){

	if(argc!=2&&argc!=3){
		usage();
		return -1;
	}
	int tcp_port = 8080;
	int ssl_port = 4433;
	if(argc==2){
		tcp_port =	atoi(argv[1]);
	}else if(argc==3){
		ssl_port = atoi(argv[2]);
	}

	int sockfd = socket(AF_INET, SOCK_STREAM,0);
	if(sockfd == -1){
		perror("socket failed\n");
		return -1;
	}
	int optval =1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,&optval, sizeof(int));


	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(tcp_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero,0x00,sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(sockaddr));
	if(res == -1){
		perror("bind failed\n");
		return -1;
	}

	res = listen(sockfd,2);
	if(res == -1){
		perror("listen failed\n");
		return -1;
	}

	while(true){
		struct sockaddr_in addr;
		socklen_t client_len = sizeof(sockaddr);
		int clientfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &client_len);
		if(clientfd<0){
			perror("error on accept\n");
			break;
		}
		printf("connected! \n");

		proxying(clientfd);
	}
	close(sockfd);
}

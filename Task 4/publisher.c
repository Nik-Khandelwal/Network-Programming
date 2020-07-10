#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/file.h>
typedef struct sockaddr SA;

#define MAXSZ 1024
int main(int argc, char* argv[]){
	if (argc != 3)
	{
		printf("usage: %s <IPaddress> <Port Number>\n",argv[0]);
		exit(1);
	}

    setvbuf(stdout, NULL, _IONBF, 0);
	int	sockfd, n;
	struct sockaddr_in	servaddr;
	char buff[1024];

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    	perror("socket error");

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
    	printf("inet_pton error\n");
	printf("..............\n");
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
    	perror("connect error");

	int topic = 0;
	while(1)
	{
		printf("1. Create a topic\n2. Send a msg\n3.Send a message file\n4. exit\nEnter choice number\n");
		
		int ch;
		scanf("%d",&ch);
		if(ch==1)
		{
			char message[512];
			printf("enter topic name:");
			scanf("%s", message);
			//fgets(message, 512, stdin);
			// printf("topic name - %s\n", message);
			memset(buff,0,1024);
			sprintf(buff,"add %s",message);
			// printf("buff : %s\n", buff);
		    send(sockfd,buff,strlen(buff),0);
            char* recvline=malloc(sizeof(char)*1024);
            if (recv(sockfd,recvline,MAXSZ,0)>0)
            	printf("topic_id - %s\n",recvline);
            sscanf(recvline, "%d", &topic); 
            fflush(stdin)
            continue;
		}
		if (ch==2)
		{
			if (topic<0)
				continue;

			char message[512];
			printf("Enter message:");
			scanf("%s", message);
			//fgets(message, 512, stdin);
			printf("The entered message is - %s\n", message);
			memset(buff,0,1024);
			sprintf(buff,"save %d %s",topic,message);
			// printf("buff : %s\n", buff);
		    send(sockfd,buff,strlen(buff),0);
            char* recvline=malloc(sizeof(char)*1024);

            if (recv(sockfd,recvline,MAXSZ,0)>0)
            	printf("%s",recvline);

		}
		if (ch==3)
		{
			if (topic<0)
				continue;

			char fname[100];
			printf("enter filename:");
			scanf("%s", fname);

			FILE *fp = fopen(fname, "r");
            if (fp != NULL) {
            	char message[512];
                while(fread(message, sizeof(message), 1, fp)>0)
                {
                	memset(buff,0,1024);
				    sprintf(buff,"save %d %s",topic,message);
				    send(sockfd,buff,strlen(buff),0);

		            char* recvline=malloc(sizeof(char)*1024);
		            if (recv(sockfd,recvline,MAXSZ,0)>0);
                }
            }
		}
		if (ch==4)
			exit(0);

	}
}


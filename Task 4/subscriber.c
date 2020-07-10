#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXSZ 1024
typedef struct sockaddr SA;



int main(int argc, char* argv[]){
	if (argc != 3)
	{
	printf("usage: %s <IPaddress> <Port Number>\n",argv[0]);
	exit(1);
	}

    setvbuf(stdout, NULL, _IONBF, 0);
	int	sockfd, n;
	struct sockaddr_in	servaddr;
	char* buff=malloc(1024);

	if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    	perror("socket error");
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port   = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
    	printf("inet_pton error\n");
	printf("......\n");
	if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
    	perror("connect error");

	int topic=0;
	while(1)
	{
		printf("1. Enter topic\n2.Request message\n3. Keep receiving messages\n4. exit\nEnter choice number\n");
		
		int ch;
		scanf("%d",&ch);
		if(ch==1)
		{
			// printf("enter topic number(>0):");
			// scanf("%d",&topic);
			// continue;

			char message[512];
			printf("Enter topic name:");
			scanf("%s", message);
			//fgets(message, 512, stdin);
			// printf("topic name - %s\n", message);
			memset(buff,0,1024);
			sprintf(buff,"get %s",message);
			printf("buff : %s\n", buff);
		    send(sockfd,buff,strlen(buff),0);
            char* recvline=malloc(sizeof(char)*1024);
            if (recv(sockfd,recvline,MAXSZ,0)>0)
            	printf("topic_id - %s\n",recvline);
            sscanf(recvline, "%d", &topic);
            if(topic == -1)
            	printf("Topic does not exist yet\n"); 
            continue;
		}
		if (ch==2)
		{
			//printf("%d %d ch\n",ch,topic);
			if (topic<0)
				continue;

			memset(buff,0,1024);
			sprintf(buff,"ask %d",topic);

		    int n=send(sockfd,buff,strlen(buff),0);
			//perror("send error:");
			printf("%d %d topic\n",topic,n);
            char* recvline=malloc(sizeof(char)*1024);
			int received=0;

            while (recv(sockfd,recvline,MAXSZ,0)<=0 );
            printf("%s\n",recvline );
		
		}
		if (ch==3)
		{
			if (topic<0)
				continue;

            while(1)
            {
            	memset(buff,0,1024);
				sprintf(buff,"ask %d",topic);
			    send(sockfd,buff,strlen(buff),0);
	            char* recvline=malloc(sizeof(char)*1024);
	            
	            if (recv(sockfd,recvline,MAXSZ,0)>0)
	            	printf("%s\n",recvline );
            }
		}
		if (ch==4)
			exit(0);

	}
}

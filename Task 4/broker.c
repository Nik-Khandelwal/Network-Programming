#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ipc.h> 
#include <sys/msg.h> 

#define MESSAGE_TIME_LIMIT 60
#define MAXPENDING 10
#define BULK_LIMIT 10

typedef struct sockaddr SA;

struct mesg_buffer { 
    long mesg_type; 
    char mesg_text[512]; 
    clock_t begin;
	char mtext;
};

char** parse(char* input) {
  int i = 0;
  char** arr_arg = malloc(sizeof(char) * 512*11);
  const char* space = " ";
  char* arg = strtok(input, space);

  while((arg != NULL)&&(strcmp(arg,"\n")!=0)) {
	arr_arg[i] = arg;
	i++;
	arg = strtok(NULL, space);
  }
  arr_arg[i] = NULL;
  return arr_arg;
}
char** parsemsg(char* input) {
  int i = 0;
  char** arr_arg = malloc(sizeof(char) * 512*11);
  const char* space = "$";
  char* arg = strtok(input, space);

  while((arg != NULL)&&(strcmp(arg,"\n")!=0)) {
	arr_arg[i] = arg;
	i++;
	arg = strtok(NULL, space);
  }
  arr_arg[i] = NULL;
  return arr_arg;
}

int main(int argc, char* argv[])
{
	if (argc != 4)
	{
	printf("usage: %s <next broker IPaddress> <broker Port Number> <local port>\n",argv[0]);
	exit(1);
	}

    setvbuf(stdout, NULL, _IONBF, 0);
	struct sockaddr_in echoServAddr;
	struct sockaddr_in echoClntAddr;
	unsigned short echoServPort;	 
	unsigned int clntLen;       	 

	echoServPort = atoi(argv[3]);
    int servSock;
	//printf("1");
	if ((servSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    	perror("socket() failed");
	//printf("2");
	memset(&echoServAddr, 0, sizeof(echoServAddr));   
	echoServAddr.sin_family = AF_INET;               	 
	echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	echoServAddr.sin_port = htons(echoServPort); 	 

	if (bind(servSock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
    	perror("bind() failed");
	//printf("3");
	if (listen(servSock, MAXPENDING) < 0)
    	perror("listen() failed");
	//printf("4");
    int msgid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
	for (;;)
	{
		printf("5\n");
    	clntLen = sizeof(echoClntAddr);
    	int tmpsock;
    	if ((tmpsock = accept(servSock, (struct sockaddr *) &echoClntAddr,	&clntLen)) < 0)
        	perror("accept() failed");
		//printf("6\n");
		//printf(" tmp :%d \n", tmpsock);
    	if(fork() == 0)
    	{	
    		close(servSock);
			//printf("7\n");
    		//char* recvline=malloc(sizeof(char)*512*2);
			char recvline[1024];
    		char* recv_cp=malloc(sizeof(char)*512*2);
    		memset(recvline,0,512*2);
    		char** args;
    		strcpy(recv_cp,recvline);
			int n;

   		 	while ((n=recv(tmpsock,recvline,1024,0))!=-1)
   		 	{

				//send(tmpsock,"bytes read",10,0);
   		 		args=parse(recvline);
				//printf("bytes read: %d :%s %s\n",n,recv_cp,args[2]);

   		 		if(strcmp(args[0],"add")==0)
   		 		{
   		 			int f= 0;
					char tmp_top[20];
					int tmp =-1;
					int prev = tmp;

   		 			printf("1\n");
 					FILE* fptr = fopen("topics.txt","w");
	 				if(fptr == NULL)
				    {
				      printf("Error!");   
				      exit(1);             
				    }

					while (fscanf(fptr,"%s %d\n",tmp_top,&tmp)==2) 
        			{
						if(strcmp(tmp_top,args[1])==0)
						{
							f = 1;
							break;
						}
						prev = tmp;
					}
					fclose(fptr);
					fflush(stdin);

					printf("22\n");

				    if(f != 1)
				    {
				    	fptr = fopen("topics.txt","a");
				    	fprintf(fptr, "%s %d",args[1],prev+1);
				    	fclose(fptr);

				    	char* buff=malloc(sizeof(char)*512);
	   		 			sprintf(buff,"%d",tmp+1);
	   		 			send(tmpsock,buff,strlen(buff),0);
	   		 
	   		 			int	sockfd, n;
						struct sockaddr_in	servaddr;
						if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
					    	perror("socket error");
						bzero(&servaddr, sizeof(servaddr));
						servaddr.sin_family = AF_INET;
						servaddr.sin_port   = htons(atoi(argv[2]));
						if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
					    	printf("inet_pton error\n");
						printf("trying to connect\n");
						if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
					    	perror("connect error");

					    buff=malloc(sizeof(char)*512);
	   		 			sprintf(buff,"add %s",args[1]);

	   		 			send(sockfd,buff,strlen(buff),0);
	   		 			char* recvline=malloc(sizeof(char)*1024);
            			if (recv(sockfd,recvline,1024,0)>0)
            				printf("%s",recvline);
					}
					else
					{
						printf(".\n");
						char* buff=malloc(sizeof(char)*512);
	   		 			sprintf(buff,"%d",tmp);
	   		 			send(tmpsock,buff,strlen(buff),0);
					}
					continue;
   		 		}

   		 		if(strcmp(args[0],"get")==0)
   		 		{
   		 			int f= 0;
					char tmp_top[20];
					int tmp =-1;

 					FILE* fptr = fopen("topics.txt","r");
	 				if(fptr == NULL)
				    {
				      printf("Error!");   
				      exit(1);             
				    }

					while (fscanf(fptr,"%s %d\n",tmp_top,&tmp)==2) 
        			{
						if(strcmp(tmp_top,args[1])==0)
						{
							f = 1;
							break;
						}
					}
				    if(!f)
				    	tmp = -1;

				    fclose(fptr);
   		 			char* buff=malloc(sizeof(char)*512);
   		 			sprintf(buff,"%d",tmp);
   		 			send(tmpsock,buff,strlen(buff),0);
   		 			continue;
   		 		}

   		 		if (strcmp(args[0],"save")==0)
   		 		{
   		 			struct mesg_buffer msg;
   		 			msg.mesg_type=atoi(args[1]);
   		 			strcpy(msg.mesg_text,args[2]);
					strcat(msg.mesg_text,"\0");
   		 			msg.begin=clock();
					msg.mtext='\0';
   		 			msgsnd(msgid, &msg, sizeof(msg), 0);
					//perror("send");
   		 			char* buff=malloc(sizeof(char)*512);
   		 			buff="message saved";
   		 			send(tmpsock,buff,strlen(buff),0);
					//printf("save finished: .%s.\n",msg.mesg_text);
   		 			continue;
   		 		}
   		 		if (strcmp(args[0],"ask")==0)
   		 		{
   		 			struct mesg_buffer message;
   		 			while(1)
   		 			{
						//printf("topic %d size %ld %ld",atoi(args[1]),sizeof(message),sizeof(message.mesg_text));
	   		 			if (msgrcv(msgid, &(message.mesg_type), sizeof(message), atoi(args[1]), IPC_NOWAIT)==-1)
	   		 			{
							//printf("inside -1 part\n");
							//exit(0);
	   		 				int	sockfd, n;
							struct sockaddr_in	servaddr;
							if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
						    	perror("socket error");
							bzero(&servaddr, sizeof(servaddr));
							servaddr.sin_family = AF_INET;
							servaddr.sin_port   = htons(atoi(argv[2]));
							if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
						    	printf("inet_pton error\n");
							printf(".........\n");
							if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
						    	perror("connect error");

						    char* buff=malloc(sizeof(char)*512);
		   		 			sprintf(buff,"askmore %s",args[1]);
		   		 			send(sockfd,buff,strlen(buff),0);
		   		 			char* receive=malloc(sizeof(char)*512*11);
	    					memset(receive,0,512*11);

	    					if (recv(sockfd,receive,512*11,0))
	    					{
	    						char** parsed=parsemsg(receive);
	    						send(tmpsock,parsed[0],strlen(parsed[0]),0);
	    						int i=1;

	    						while(parsed[i]!=NULL)
	    						{
	    							struct mesg_buffer msg;

				   		 			msg.mesg_type=atoi(args[1]);
				   		 			strcpy(msg.mesg_text,parsed[i]);
				   		 			msg.begin=clock();
				   		 			int status=msgsnd(msgid, &msg, sizeof(msg), 0);
									printf("saving msg: %s\n",msg.mesg_text);
									i++;
									//perror("msg save-1:");
	    						}
	    					}
	    					close(sockfd);
	    					break;
	   		 			}
	   		 			else
	   		 			{
							if (message.begin-clock()/CLOCKS_PER_SEC < MESSAGE_TIME_LIMIT*1000)
	   		 				{
	   		 					send(tmpsock,message.mesg_text,strlen(message.mesg_text),0);
	   		 					break;
	   		 				}
	   		 			}
   		 			}
   		 			continue;
   		 		}
   		 		if (strcmp(args[0],"askmore")==0)
   		 		{
   		 			struct mesg_buffer message;
                    while(1)
                    {
                        
	   		 			if (msgrcv(msgid, &message, sizeof(message), atoi(args[1]), IPC_NOWAIT)==-1)
	   		 			{
	   		 				int	sockfd, n;
							struct sockaddr_in	servaddr;
							if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
						    	perror("socket error");
							bzero(&servaddr, sizeof(servaddr));
							servaddr.sin_family = AF_INET;
							servaddr.sin_port   = htons(atoi(argv[2]));
							if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
						    	printf("inet_pton error\n");
							printf("trying to connect\n");
							if (connect(sockfd, (SA *) &servaddr, sizeof(servaddr)) < 0)
						    	perror("connect error");

						    char* buff=malloc(sizeof(char)*512);
		   		 			sprintf(buff,"askmore %s",args[1]);
		   		 			send(sockfd,buff,strlen(buff),0);
		   		 			char* receive=malloc(sizeof(char)*512*11);
	    					memset(receive,0,512*11);

	    					if (recv(sockfd,receive,512*11,0))
	    					{
	    						send(tmpsock,receive,strlen(receive),0);
	    					}
	    					close(sockfd);
	    					break;
	   		 			}
	   		 			else{

	   		 				if (message.begin-clock()/CLOCKS_PER_SEC < MESSAGE_TIME_LIMIT*1000)
	   		 				{
		   		 				char* send_message=malloc(sizeof(char)*512*11);
		   		 				sprintf(send_message,"%s",message.mesg_text);
		   		 				int count=1;

		   		 				while((count<BULK_LIMIT) && (msgrcv(msgid, &message, sizeof(message), atoi(args[1]), IPC_NOWAIT)!=-1))
		   		 				{
		   		 					if(message.begin-clock()/CLOCKS_PER_SEC < MESSAGE_TIME_LIMIT*1000)
		   		 					{
			   		 					strcat(send_message,"$");
			   		 					strcat(send_message,message.mesg_text);
			   		 					count++;
			   		 				}   		 					
		   		 				}
		   		 				send(tmpsock,send_message,strlen(send_message),0);
		   		 				break;
		   		 			}
	   		 			}
	   		 		}
   		 			continue;
   		 		}
   		 	}
   		 	exit(0);
   		}//end of fork()
   	}
   	return 1;
}

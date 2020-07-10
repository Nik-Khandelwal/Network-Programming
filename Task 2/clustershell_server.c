#include <stdio.h>      /* for printf() and fprintf() */
#include <sys/socket.h> /* for socket(), connect(), send(), and recv() */
#include <arpa/inet.h>  /* for sockaddr_in and inet_addr() */
#include <stdlib.h>     /* for atoi() and exit() */
#include <string.h>     /* for memset() */
#include <unistd.h>     /* for close() */
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define PENDING 5

int N;

void error_Handler(char buffer[])
{
    perror(buffer);
    exit(0);
}

void Sig_Handler(int sig_num)
{
    for(int j=0;j<N;j++)
        {
            int msgid;
            key_t key = ftok("./clustershell_client.c", j);
            if((msgid = msgget(key, 0666)) == -1)
                continue;
            msgctl(msgid, IPC_RMID, NULL);
        }

    printf("Shutting Down ... \n");
    exit(0);
}


struct message
{
    long type;
    char input[4096];
    char cmd[128];
};


void HandleTCPClient(int clntSock_list[],int node_num,char nodes_list[])
{
    int clntSock = clntSock_list[node_num];
    int pid = fork();
    if(pid < 0)
        error_Handler("Fork");

    if(pid == 0)
    {
        char shellcmdbuffer[128];
        char shelloutput[4096];
        char cmd[128];
        char node[10];
        key_t key;
        int msgid;

        int recvMsgSize;                    

        for(;;)
        {
            memset(shellcmdbuffer,0,strlen(shellcmdbuffer));
            memset(cmd,0,strlen(cmd));
            memset(shelloutput,0,strlen(shelloutput));
            memset(node,0,strlen(node));

            recvMsgSize = recv(clntSock, shellcmdbuffer, 128, 0);
            if(recvMsgSize == 0)
                continue;
            shellcmdbuffer[recvMsgSize] = '\0';

            char *p = strstr(shellcmdbuffer,"nodes");
            if(p)
            {
                strcpy(shelloutput,nodes_list);
                sprintf(node,"%d",node_num);
            }
            else if(strchr(shellcmdbuffer,'@') != NULL)
            {
                sscanf( shellcmdbuffer, "%[^@] @ %*c%s . %[^\n]", shelloutput, node, cmd);
            }
            else
            {
                sscanf( shellcmdbuffer, "%*c%s . %[^\n]", node, cmd);
                shelloutput[0] = '\0';

                char txt[10] = " | n";
                sprintf(txt+4, "%d", node_num);
                strcat(cmd,txt);
                strcat(cmd," . ");

            }

            // printf("%s\n",node);
            if(node[0] == '*')
            {
                for(int j=0;j<=N;j++)
                {
                    key = ftok("./clustershell_client.c", j);
                    if((msgid = msgget(key, 0666)) == -1)
                        continue;

                    struct message msg;

                    msg.type = 1;
                    strcpy(msg.input,shelloutput);
                    strcpy(msg.cmd,cmd);

                    msgsnd(msgid, &(msg.type), sizeof(msg), 0);
                }
            }
            else
            {
                key = ftok("./clustershell_client.c", atoi(node));
                msgid = msgget(key, 0666 | IPC_CREAT);

                struct message msg;
                msg.type = 1;
                strcpy(msg.input,shelloutput);
                strcpy(msg.cmd,cmd);

                msgsnd(msgid, &(msg.type), sizeof(msg), 0);
            }

        }
        close(clntSock);
        exit(0);
    }
    else
    {
        char msgqBuffer[4096];
        key_t key = ftok("./clustershell_client.c", node_num);
        int msgid = msgget(key, 0666 | IPC_CREAT);
        struct message msg;

        for (;;)
        {

            msgrcv (msgid, &(msg.type), sizeof (msg), 0, 0);

            memset(msgqBuffer,0,strlen(msgqBuffer));
            if(strlen(msg.input) != 0)
            {
                strcpy(msgqBuffer,msg.input);
                strcat(msgqBuffer," @ ");
            }

            if(strlen(msg.cmd) != 0)
            {
                strcat(msgqBuffer,msg.cmd);
            }

            send(clntSock, msgqBuffer, strlen(msgqBuffer), 0);
        }
    }    
}


int main(int argc, char *argv[])
{
    signal(SIGINT, Sig_Handler);

    int servSock;                   
    struct sockaddr_in ServAddr;
    struct sockaddr_in ClntAddr;
    unsigned short ServPort;     
    unsigned int clnt_Len;            

    if (argc != 2)     
    {
        fprintf(stderr, "Usage:  %s <Server Port>\n", argv[0]);
        exit(1);
    }

    ServPort = atoi(argv[1]);  

    char path[1024];
    printf("Enter path of config file\n");
    scanf("%s",path);

    FILE *fptr;
    if ((fptr = fopen(path, "r")) == NULL)
        error_Handler("Error! opening config file");

    N = 0;
    for (char c = getc(fptr); c != EOF; c = getc(fptr))
        if (c == '\n')
            N = N + 1;
    fseek(fptr, 0, SEEK_SET);
    printf("\n\nThe cluster shell can have following nodes  - \n");

    char ip[N+1][20];
    int tmp;
    while(!feof(fptr))
    {
        fscanf(fptr,"%*c%d ",&tmp);
        printf("n%d - ",tmp);
        fscanf(fptr,"%s\n",ip[tmp]);
        printf("%s\n",ip[tmp]);        
    }

    printf("\n");

    int clntSock_list[N+1];
    char nodes_list[25*N];

    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        error_Handler("socket() failed");
      

    memset(&ServAddr, 0, sizeof(ServAddr));   

    ServAddr.sin_family = AF_INET;                    

    ServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    ServAddr.sin_port = htons(ServPort);      


    if (bind(servSock, (struct sockaddr *) &ServAddr, sizeof(ServAddr)) < 0)
        error_Handler("bind() failed");


    if (listen(servSock, PENDING) < 0)
        error_Handler("listen() failed");

    printf("\n\n--------------------------------------------------------------------------\n");
    printf("                              SERVER RUNNING                              \n");
    printf("--------------------------------------------------------------------------\n\n");
    int node_num;
    memset(nodes_list,0,strlen(nodes_list));
    for (;;)
    {
        clnt_Len = sizeof(ClntAddr);
        int tmpsock;

        if ((tmpsock = accept(servSock, (struct sockaddr *) &ClntAddr,&clnt_Len)) < 0)
            error_Handler("accept() failed");

        for(node_num = 0 ; node_num<=N ; node_num++)
            if(strcmp(inet_ntoa(ClntAddr.sin_addr),ip[node_num]) == 0)
                break;


        clntSock_list[node_num] = tmpsock;
        char temp[50];
        sprintf(temp, "n%d - %s\n",node_num,inet_ntoa(ClntAddr.sin_addr));
        // printf("pnl - %s\n",nodes_list);
        strcat(nodes_list,temp);
        // printf("nnl - %s\n",nodes_list);
        if(fork() == 0)
        {

            printf("Client n%d - %s has connected\n",node_num, inet_ntoa(ClntAddr.sin_addr));

            HandleTCPClient(clntSock_list,node_num,nodes_list);

            printf("Client n%d - %s has disconnected\n",node_num, inet_ntoa(ClntAddr.sin_addr));

            char node[10];
            sprintf(node, "n%d",node_num);
            char *p = strstr(nodes_list,node);
            if(p)
            {
                *p = '\0';
                while(*p != '\n')
                p++;
                p++;
                strcat(nodes_list,p);
            }
            exit(0);
        }
        else
        {
            close(clntSock_list[node_num]);
        }
    }
}


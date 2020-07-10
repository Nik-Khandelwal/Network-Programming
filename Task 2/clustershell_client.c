#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXSZ 4096
#define BUFFSZ 1024

int sockfd;

void error_Handler(char buffer[])
{
    perror(buffer);
    exit(0);
}

void Sig_Handler(int sig_num)
{
    close(sockfd);
    printf("Disconnecting from server ...\n");
    exit(0);
}


char** parse_buffer(char* input)
{
    int i = 0;
    char** arr_arg = malloc(sizeof(char) * BUFFSZ);
    const char* space = " ";
    char* arg = strtok(input, space);

    while((arg != NULL)&&(strcmp(arg,"\n")!=0))
    {
        arr_arg[i] = arg;
        i++;
        arg = strtok(NULL, space);
    }
    arr_arg[i] = NULL;
    return arr_arg;
}

int main(int argc, char **argv)
{   
    signal(SIGINT, Sig_Handler);

    setvbuf(stdout, NULL, _IONBF, 0);
    int n;
    char    recvline[MAXSZ];
    struct sockaddr_in    servaddr;
    struct pollfd fds[2];
    char buff[BUFFSZ];

    if (argc != 3)
        {
        printf("usage: a.out <IPaddress> <Port Number>\n");
        exit(1);
        }

    if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error_Handler("socket error");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0)
        printf("inet_pton error\n");

    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        error_Handler("connect error");

    printf("\n\n--------------------------------------------------------------------------\n");
    printf("                            CONNECTED TO SERVER                           \n");
    printf("--------------------------------------------------------------------------\n\n");

    if (fork()==0)
    {
        setvbuf(stdout, NULL, _IONBF, 0);
        for(;;)
        {
            memset(buff,0,strlen(buff));

            while (read(STDIN_FILENO,buff,BUFFSZ)<=0);

            char* buff2=malloc(sizeof(char)*BUFFSZ);
            strcpy(buff2,buff);
            {
                int l = strlen (buff2);
                if (l > 0 && buff2[l - 1] == '\n')
                    buff2[l - 1] = '\0';

                char** args = parse_buffer(buff2);
                int i = 0;
                int sent=0;
                while(args[i] != NULL)
                {
                    if(strcmp(args[i], "nodes") == 0)
                    {
                        send(sockfd,buff,strlen(buff),0);
                        sent=1;
                        break;
                    }
                    else if(strcmp(args[i], ".") == 0)
                    {
                        send(sockfd,buff,strlen(buff),0);
                        sent=1;
                        break;
                    }
                    i++;
                }

                if (sent==0)
                {
                    if (fork()==0)
                    {
                        setvbuf(stdout, NULL, _IONBF, 0);
                        execvp(args[0],args);
                    }
                }
            }
        }
        exit(0);
    }
    else
    {
        setvbuf(stdout, NULL, _IONBF, 0);
        for(;;)
        {
            memset(recvline,0, strlen(recvline));
            if (recv(sockfd,recvline,MAXSZ,0)>0)
            {
                char* b=malloc(sizeof(char)*BUFFSZ);
                strcpy(b,recvline);
                char** args=parse_buffer(b);
                int symIn=-1, pipeIn=-1,display=0;
                int i=0;

                while(args[i]!=NULL)
                {
                    if ((strcmp(args[i],"@")==0)&&(symIn==-1))
                    {
                        symIn=i;
                        if (args[i+1]==NULL)
                            display=1;
                    }
                    if ((strcmp(args[i],"|")==0)&&(pipeIn==-1))
                    {
                        pipeIn=i;
                    }
                    if ((symIn!=-1)&&(pipeIn!=-1))
                        break;
                    i++;
                }

                if (display==1)
                {
                    int j=0;
                    while(j<symIn) 
                    {
                        fputs(args[j],stdout);
                        fputs(" ",stdout);
                        j++;
                    }
                    printf("\n\n");
                }
                else if ((symIn!=-1)&&(pipeIn==-1))
                {
                    int fd[2];
                    pipe(fd);
                    int fd2[2];
                    pipe(fd2);

                    if (fork()==0)
                    {
                        dup2(fd[0], STDIN_FILENO);
                        close(fd[1]);
                        dup2(fd2[1], STDOUT_FILENO);
                        close(fd2[0]);
                        execvp(args[i+1],&args[i+1]);
                    }

                    int j=0;
                    while(j<symIn)
                    {
                        write(fd[1],args[j],strlen(args[j]));
                        write(fd[1]," ",1);
                        j++;
                    }

                    close(fd[0]);
                    close(fd2[1]);
                    close(fd[1]);
                    wait(NULL);
                    char* output=malloc(sizeof(char)*MAXSZ);

                    read(fd2[0],output,MAXSZ);
                    strcat(output," @ ");
                    send(sockfd,output,strlen(output),0);
                    close(fd2[0]);    
                }
                else if ((symIn==-1)&&(pipeIn==-1))
                {
                    if (strcmp(args[0],"cd")==0)
                    {
                        char cwd[256];
                        getcwd(cwd,sizeof(cwd));
                        strcat(cwd,"/");
                        strcat(cwd,args[1]);
                        chdir(cwd);
                    }
                    else
                    {
                        int fd[2];
                        pipe(fd);
                        if(fork()==0)
                        {
                            dup2(fd[1], STDOUT_FILENO);
                            close(fd[0]);
                            execvp(args[0],args);
                        }

                        close(fd[1]);
                        wait(NULL);
                        char* output=malloc(sizeof(char)*MAXSZ);

                        read(fd[0],output,MAXSZ);

                        send(sockfd,output,strlen(output),0);
                        close(fd[0]);
                    }
                }
                else if ((symIn==-1)&&(pipeIn!=-1))
                {
                    if (strcmp(args[0],"cd")==0)
                    {
                        char cwd[256];
                        getcwd(cwd,sizeof(cwd));
                        strcat(cwd,"/");
                        strcat(cwd,args[1]);
                        chdir(cwd);
                    }
                    else
                    {
                        char* cmd=malloc(sizeof(char)*BUFFSZ);
                        args[pipeIn]=NULL;
                        int j=pipeIn+1;

                        while(args[j]!=NULL)
                        {
                            strcat(cmd,args[j]);
                            strcat(cmd," ");
                            args[j]=NULL;
                            j++;
                        }
                        int fd[2];
                        pipe(fd);

                        if(fork()==0)
                        {
                            dup2(fd[1], STDOUT_FILENO);
                            close(fd[0]);
                            execvp(args[0],args);
                        }

                        close(fd[1]);
                        wait(NULL);
                        char* output=malloc(sizeof(char)*MAXSZ);

                        read(fd[0],output,MAXSZ);
                        strcat(output," @ ");
                        strcat(output,cmd);
                        send(sockfd,output,strlen(output),0);
                        close(fd[0]);
                    }
                }
                else if ((symIn!=-1)&&(pipeIn!=-1))
                {
                    char* cmd=malloc(sizeof(char)*BUFFSZ);
                    args[pipeIn]=NULL;
                    int j=pipeIn+1;

                    while(args[j]!=NULL)
                    {
                        strcat(cmd,args[j]);
                        strcat(cmd," ");
                        args[j]=NULL;
                        j++;
                    }

                    int fd[2];
                    pipe(fd);
                    int fd2[2];
                    pipe(fd2);

                    if (fork()==0)
                    {
                        dup2(fd[0], STDIN_FILENO);
                        close(fd[1]);
                        dup2(fd2[1], STDOUT_FILENO);
                        close(fd2[0]);
                        execvp(args[symIn+1],&args[symIn+1]);
                    }
                    j=0;

                    while(j<symIn)
                    {
                        write(fd[1],args[j],strlen(args[j]));
                        write(fd[1]," ",1);
                        j++;
                    }

                    close(fd[0]);
                    close(fd2[1]);
                    close(fd[1]);
                    wait(NULL);
                    char* output=malloc(sizeof(char)*MAXSZ);

                    read(fd2[0],output,MAXSZ);
                    strcat(output," @ ");
                    strcat(output,cmd);
                    send(sockfd,output,strlen(output),0);
                    close(fd2[0]);
                }
            }
        }
    }

    exit(0);
}















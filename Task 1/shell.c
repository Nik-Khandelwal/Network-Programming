#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#define MSG_SIZE  4096
#define BUFFSIZE  1024

int first_Pipe(char** ar1, char** ar2);
int second_Pipe(char** ar1, char** ar2);
int execute(char** arr);
char secs[20][100];


struct msg_buffer { 
    long mtype; 
    char mesg_text[1024]; 
    char mtext;
};


void sig_handler(int signo)
{
  pid_t pid3;
  if((pid3=fork())==0)
  {		
    exit(0);
  }
  else
  {
    pid_t pd = tcgetpgrp(STDIN_FILENO);
    setpgid(pid3, 0);
    tcsetpgrp(STDIN_FILENO, pid3);
    waitpid(pid3, NULL, 0);
    tcsetpgrp(STDIN_FILENO, getpid());
    char cur_dir[1024];
    getcwd(cur_dir, sizeof(cur_dir));
    char* g = (char*)malloc(sizeof(char)*1024);
    strcpy(g,"$Nik_viv:~");
    g = strcat(g,cur_dir);
    g = strcat(g , "/>");
    write(1,g,1024);
  }
}

int makeMQ(char** ar1, char** ar2) {
  pid_t pid1, pid2;
  int fd[2];

  pipe(fd);
  int fd2[2];

  pipe(fd2);
   int msqid;
   int len;
   
   if ((msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }
  printf("Fd of pipes used with msg queue is: %d %d %d %d\n",fd[0],fd[1],fd2[0],fd2[1] );


  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {
   
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);

  }
  else {
    pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
   
    char buffer[1024];
    ssize_t read_size;
    struct msg_buffer buf;
    
    close(fd[1]);
    while(read(fd[0], buf.mesg_text, sizeof buf.mesg_text)> 0) {
      buf.mtype = 1;
      buf.mtext='\0';
      if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1) 
      perror("msgsnd");
    }
    close(fd[0]);
    strcpy(buf.mesg_text, "end");
    buf.mtype = 1;
    buf.mtext='\0';
   if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1)
      perror("msgsnd");
 
  for(;;) { 
    if (msgrcv(msqid, &buf, sizeof(buf), 0, 0) == -1) {
       perror("msgrcv");
       exit(1);
    }

    if (strcmp(buf.mesg_text,"end") == 0)
      break;
    write(fd2[1],buf.mesg_text,strlen(buf.mesg_text));
   }
   close(fd2[1]);
    
    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {
      dup2(fd2[0], STDIN_FILENO);
      close(fd2[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {
    
close(fd2[0]);
      if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
      }
      
      
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}

int dobMQ(char** ar1, char** ar2) {
  pid_t pid1, pid2;
 
  int i=0;int j=0;
  while(strcmp(ar2[i],",")!=0){
   
    i++;
  }
  
  while(ar2[i]!=NULL){
    
    ar2[i]=NULL;
    
    i++;
  }
 

  int fd[2];
  pipe(fd);
  int fd2[2];
  pipe(fd2);
   int msqid;
   int len;
   
   if ((msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT)) == -1) {
      perror("msgget");
      exit(1);
   }
  printf("Fd of pipes used with msg queue is: %d %d %d %d\n",fd[0],fd[1],fd2[0],fd2[1] );

  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {

    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);
  }
  else {
    pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());

    char buffer[1024];
    ssize_t read_size;
    struct msg_buffer buf;
    
    close(fd[1]);
    while(read(fd[0], buf.mesg_text, sizeof buf.mesg_text)> 0) {
      buf.mtype = 1;
      buf.mtext='\0';
      if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1) 
      perror("msgsnd");
    }
    close(fd[0]);
    strcpy(buf.mesg_text, "end");
    buf.mtype = 1;
    buf.mtext='\0';
   if (msgsnd(msqid, &buf, sizeof(buf), 0) == -1)
      perror("msgsnd");
 
  for(;;) { 
    if (msgrcv(msqid, &buf, sizeof(buf), 0, 0) == -1) {
       perror("msgrcv");
       exit(1);
    }
 
    if (strcmp(buf.mesg_text,"end") == 0)
      break;
    write(fd2[1],buf.mesg_text,strlen(buf.mesg_text));
   }
   close(fd2[1]);
    
    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {
     
      dup2(fd2[0], STDIN_FILENO);
      close(fd2[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {
     
    close(fd2[0]);
      if (msgctl(msqid, IPC_RMID, NULL) == -1) {
      perror("msgctl");
      exit(1);
      }
     
     
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}

int makeSM(char** ar1, char** ar2) {
  pid_t pid1, pid2;
  int fd[2];

  pipe(fd);
  int fd2[2];

  pipe(fd2);

   int len;
   int shmid = shmget(IPC_PRIVATE,4096,0666|IPC_CREAT); 
  
    char *str = (char*) shmat(shmid,(void*)0,0);

  printf("Fd of pipes used with shared memory is: %d %d %d %d\n",fd[0],fd[1],fd2[0],fd2[1] );

  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);

  }
  else {
    pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
  
    

    close(fd[1]);
    read(fd[0], str, 4096);
     
    
    close(fd[0]);
   
    write(fd2[1],str,strlen(str));
 
   close(fd2[1]);
    
    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {
      dup2(fd2[0], STDIN_FILENO);
      close(fd2[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {
     
close(fd2[0]);
      
    shmdt(str); 
    
    
    shmctl(shmid,IPC_RMID,NULL);
      
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}

int dobSM(char** ar1, char** ar2) {
  pid_t pid1, pid2;
 
  int i=0;int j=0;
  while(strcmp(ar2[i],",")!=0){
  
    i++;
  }
  
  while(ar2[i]!=NULL){
    
    ar2[i]=NULL;
    
    i++;
  }
 
 
  int fd[2];
 
  pipe(fd);
  int fd2[2];
 
  pipe(fd2);
   
   int len;
   
   int shmid = shmget(IPC_PRIVATE,4096,0666|IPC_CREAT); 
  
    
    char *str = (char*) shmat(shmid,(void*)0,0);
   
  printf("Fd of pipes used with shared memory is: %d %d %d %d\n",fd[0],fd[1],fd2[0],fd2[1] );

 
  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {

    
    dup2(fd[1], STDOUT_FILENO);
    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);
  }
  else {
    pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
   
    
    close(fd[1]);
   
    read(fd[0], str, 4096);
  
    close(fd[0]);
  
    write(fd2[1],str,strlen(str));
   
   close(fd2[1]);
  
    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {
     
      dup2(fd2[0], STDIN_FILENO);
      close(fd2[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {
     
     close(fd2[0]);
     
    shmdt(str); 
    
    shmctl(shmid,IPC_RMID,NULL);
     
     
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}


int main(int argc, char*argv[])
{
  system("clear");
  pid_t pid;
  signal(SIGINT,sig_handler);
  signal(SIGTTOU, SIG_IGN);
  for(int i=0;i<10;i++)
  {
    strcpy(secs[i],"NULL");
  }

  pid = tcgetpgrp(STDOUT_FILENO);

  setpgid(getpid(), 0);
  tcsetpgrp(STDOUT_FILENO, getpid());

  int a = 1;
  while(a != 0)
  {
    char cur_dir[1024];
    getcwd(cur_dir, sizeof(cur_dir));
    printf("$Nik_viv:~%s/>", cur_dir);
    
  char* input = malloc(sizeof(char) * BUFFSIZE);
  int pos = 0;
  char c;
  while((c = getchar()) != '\n' && c != EOF) 
  {
    input[pos] = c;
    pos++;
    if(pos >= BUFFSIZE - 1)
    {
        printf("User input too long. Please input arr less than 1024 characters.\n");
        break;
    }
  }
  input[pos] = '\0';
    
    int i = 0;
  char** aarg = malloc(sizeof(char) * BUFFSIZE);
  const char* space = " ";
  char* arg = strtok(input, space);

  while(arg != NULL)
  {
    aarg[i] = arg;
    i++;
    arg = strtok(NULL, space);
  }
  aarg[i] = NULL;
    a = execute(aarg);
  }

  system("clear");
  return 1;
}


int execute(char** arr)
{
  int ret = 1; 

  int isRI = 0;
  int isRO = 0;
  int inBg = 0;
  int i, j;
  int stdinDup, stdoutDup, file_in, file_out;

  if(arr[0] == NULL)
  {
    return ret;
  }
  
  i = 0;
  while(arr[i] != NULL)
  {
    if((strcmp(arr[i], "&") == 0) && (arr[i+1]==NULL))
    {
      inBg = 1;
    } 
    else if(strcmp(arr[i], "quit") == 0)
    {
      return 0;
    }
    else if(strcmp(arr[i], "cd") == 0)
    {
      arr[i] = NULL;
      char cur_dir[256];
      getcwd(cur_dir,sizeof(cur_dir));
      strcat(cur_dir,"/"); 
      strcat(cur_dir,arr[i+1]);
      chdir(cur_dir);
      return ret;
    }
    else if(strcmp(arr[i], "daemonize") == 0)
    {
      arr[i] = NULL;
      pid_t pid, sid;
        
      pid = fork();
      if (pid < 0)
      {
        printf("error forking");
      }

      if (pid == 0)
      {

        umask(0);       
        sid = setsid();
        if (sid < 0)
        {
               
          printf("error creating session");
          exit(EXIT_FAILURE);
        }
        
        if ((chdir("/")) < 0) 
        {
        
          printf("error changing directory");
          exit(EXIT_FAILURE);
        }
        
        
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO); 
       
        execvp(arr[i+1], &arr[i+1]);

    }
    return ret;
    }
   else if(strcmp(arr[i], "<") == 0) {
      file_in = open(arr[i+1], O_RDONLY);
      arr[i] = NULL;
      isRI = 1;
    }
    else if(strcmp(arr[i], ">") == 0) {
      file_out = open(arr[i+1], O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR | O_CLOEXEC);
      arr[i] = NULL;
      isRO = 1;
    } 
    else if(strcmp(arr[i], ">>") == 0) {
      file_out = open(arr[i+1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR | O_CLOEXEC);
      arr[i] = NULL;
      isRO = 1;
    }
    else if(strcmp(arr[i], "|") == 0) {
      
      arr[i] = NULL;
      ret = first_Pipe(&arr[0], &arr[i+1]);
      return ret;
    }
    else if(strcmp(arr[i], "#") == 0) {
     
      arr[i] = NULL;
      ret = makeMQ(&arr[0], &arr[i+1]);
      return ret;
    }
    else if(strcmp(arr[i], "$") == 0) {
      
      arr[i] = NULL;
      ret = makeSM(&arr[0], &arr[i+1]);
      return ret;
    }
    else if(strcmp(arr[i], "||") == 0) {
      
      char** arg1 = malloc(sizeof(char) * BUFFSIZE);
      
      arr[i] = NULL;
      int j= i+1;
      
        
        while((strcmp(arr[j],","))!=0){
         j++;
        }
        int k=0;
        j++;
        while(arr[j]!=NULL){
          arg1[k]=arr[j];
          k++;
          j++;
        }
        second_Pipe(&arr[0], &arr[i+1]);
        ret = first_Pipe(&arr[0], &arg1[0]);
        
   
      return ret;
    }
    else if(strcmp(arr[i], "##") == 0) {
      
      char** arg1 = malloc(sizeof(char) * BUFFSIZE);
      
      arr[i] = NULL;
      int j= i+1;
    
        
        while((strcmp(arr[j],","))!=0){
         j++;
        }
        int k=0;
        j++;
        while(arr[j]!=NULL){
          arg1[k]=arr[j];
          k++;
          j++;
        }
        dobMQ(&arr[0], &arr[i+1]);
        ret = makeMQ(&arr[0], &arg1[0]);
        
 
      return ret;
    }
    else if(strcmp(arr[i], "$$") == 0) {
     
      char** arg1 = malloc(sizeof(char) * BUFFSIZE);
      
      arr[i] = NULL;
      int j= i+1;

        
        while((strcmp(arr[j],","))!=0){
         j++;
        }
        int k=0;
        j++;
        while(arr[j]!=NULL){
          arg1[k]=arr[j];
          k++;
          j++;
        }
        dobSM(&arr[0], &arr[i+1]);
        ret = makeSM(&arr[0], &arg1[0]);
        

      return ret;
    }
    i++;
  }


    pid_t pid;
    pid = fork();
    if(pid < 0) {
      printf("Error forking\n");
    }
    else if(pid == 0) {


      if(isRI == 1) {
    printf("fd of the file is %d\n",file_in);
    printf("remapped fd is %d\n",STDIN_FILENO);
     dup2(file_in, STDIN_FILENO);
      }
      if(isRO == 1) {
        
    printf("fd of the file is %d\n",file_out);
        printf("remapped fd is %d\n",STDOUT_FILENO);
     dup2(file_out, STDOUT_FILENO);
      }
      
      execvp(arr[0], arr);

      printf("Command or executable not recognized.\n");
      exit(0);
    }
    else {

      if(inBg == 0) {
        pid_t status;
        pid_t pd = tcgetpgrp(STDIN_FILENO);
      
      setpgid(pid, 0);
      tcsetpgrp(STDIN_FILENO, pid);
     
        waitpid(pid, &status, 0);
    if(WIFEXITED(status)){
    printf("pid:%d ,Executed Process Terminated Normally\n",pid);
    }
    else if(WIFSIGNALED(status)){
    printf("pid:%d ,Executed Process Terminated due to User Interruption\n",pid);
    }
    else if(WCOREDUMP(status)){
    printf("pid:%d ,Executed Process Terminated due to Core Dump\n",pid);
        }
        
        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    if(isRI == 1){
                close(file_in);}
        if(isRO == 1){close(file_out);}
        
      }
    }
  
  return ret;
}



int first_Pipe(char** ar1, char** ar2) {
  pid_t pid1, pid2;
  int fd[2];

  pipe(fd);
  printf("Fd's of pipe is: %d,%d\n",fd[0],fd[1] );


  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {

    dup2(fd[1], STDOUT_FILENO);

    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);

  }
  else {
      pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());

    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {

      dup2(fd[0], STDIN_FILENO);

      close(fd[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {

      close(fd[0]);
      close(fd[1]);

      
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}

int second_Pipe(char** ar1, char** ar2) {
  pid_t pid1, pid2;
  int fd[2];
  printf("Fd's of pipe is: %d,%d\n",fd[0],fd[1] );
 
  int i=0;int j=0;
  while(strcmp(ar2[i],",")!=0){
 
    i++;
  }
  
  while(ar2[i]!=NULL){
    
    ar2[i]=NULL;
    
    i++;
  }
 
  pipe(fd);
  

  pid1 = fork();
  if(pid1 < 0) {
    printf("Error forking.\n");
  }
  else if(pid1 == 0) {


    dup2(fd[1], STDOUT_FILENO);

    close(fd[0]);
    execute(ar1);
    printf("Pid of process: %d\n",getpid());
    exit(0);
  }
  else {
      pid_t pd = tcgetpgrp(STDIN_FILENO);
      setpgid(pid1, 0);
      tcsetpgrp(STDIN_FILENO, pid1);
      waitpid(pid1, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());

    pid2 = fork();
    if(pid2 < 0) {
      printf("Error forking.\n");
    }
    else if(pid2 == 0) {

      dup2(fd[0], STDIN_FILENO);

      close(fd[1]);
      execute(ar2);
      printf("Pid of process: %d\n",getpid());
      exit(0);
    }
    else {

      close(fd[0]);
      close(fd[1]);
     
      setpgid(pid2, 0);
      tcsetpgrp(STDIN_FILENO, pid2);
      waitpid(pid2, NULL, 0);
      signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(STDIN_FILENO, getpid());
    }
  }
  return 1;
}



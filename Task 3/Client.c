#include <stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include<netinet/in.h>
#include<fcntl.h>
#include<sys/sendfile.h>

#define false 0
#define true 1
#define TCP_BUFFER_SIZE 2047
#define MAX_ARRAY_SIZE (1<<20) //1GB Array

//Global Variables
char *FNS_IP;
int FNS_PORT;
int FNS_sfd;

int num_FDS = -1;
char **FDS_IP;
int *FDS_PORT;
int *FDS_sfd;
//for IPs, PORTs, Socket FDs


char *allocmem(int size){
    char *cstr = (char *)(calloc(size+1,sizeof(char)));
    return cstr;
}


char * trim (char *str) { // remove leading and trailing spaces
    str = strdup(str);
    if(str == NULL)
        return NULL;
    int begin = 0, end = strlen(str) -1, i;
    while (isspace (str[begin])){
        begin++;
    }

    if (str[begin] != '\0'){
        while (isspace (str[end]) || str[end] == '\n'){
            end--;
        }
    }
    str[end + 1] = '\0';

    return str+begin;
}


char *randname(int len){
    char *name = (char *)(calloc(len+1,sizeof(char)));
    name[len] = '\0';
    const char cset[] = "abcdefghijklmnopqrstuvwxyz1234567890";    //size 36
    int i,idx;
    for(i=0;i<len;i++){
        idx = rand() % 36;
        name[i] = cset[idx];
    }
    return name;
}


char **strSplit(char *str, char tk){
    if(str == NULL || strcmp(str,"")==0)
        return NULL;
    char tkn[] = {tk};
    int sz = 0;

    for(int i=0;i<strlen(str);i++)
        if(str[i] == tk)
            sz++;
    sz+=2;

    char **arr = (char **)(calloc(sz,sizeof(char *)));
    char *tmpstr = strdup(str);
    int i = 0;
    char *token;
    while((token = strsep(&tmpstr,tkn)))
        arr[i++] = token;
    arr[i] = NULL;
    return arr;
}


int inBigfs(char *path){
    int len = strlen(path);
    if(len < 5)
        return false;
    else if(len == 5)
        return (strcmp(path,"bigfs")==0);
    char tmp = path[6];
    path[6] = '\0';
    if(strcmp(path,"bigfs/")==0){
        path[6] = tmp;
        return true;
    }
    else{
        path[6] = tmp;
        return false;
    }

}

int makeDir(char *dir){
    struct stat sb;
    if(stat(dir,&sb)== 0 && S_ISDIR(sb.st_mode))
        return true;
    else{
        if(mkdir(dir,0777) == 0)
            return true;
        else{
            perror("mkdir");
            return false;
        }
    }
}


long getFileSize(char *file){
    int fd = open(file,O_RDONLY);
    if(fd == -1)
        return -1;
    long size = lseek(fd,0,SEEK_END);
    close(fd);
    return size;
}


int readConfig(char *fname){
    if(fname == NULL){
        perror("Invalid Argument.\n");
        return false;
    }
    FILE *fp = fopen(fname,"r");
    if(fp == NULL){
        perror("Cannot open the given config file.\n");
        return false;
    }
    FNS_IP = allocmem(20);
    if(fscanf(fp,"%s %d\n",FNS_IP,&FNS_PORT) == EOF)
        return false;
    if(fscanf(fp,"%d\n",&num_FDS) == EOF)
        return false;
    FDS_IP = (char **)(calloc(num_FDS,sizeof(char *)));
    FDS_PORT = (int *)(calloc(num_FDS,sizeof(int)));
    for(int i = 0; i < num_FDS; i++){
        FDS_IP[i] = allocmem(20);
        if(fscanf(fp,"%s%d",FDS_IP[i],&FDS_PORT[i]) == EOF)
            return false;
    }
    return true;
}


int createConnection(){
    if(num_FDS == -1)
        return false;
    FDS_sfd = (int *)(calloc(num_FDS,sizeof(int)));
    struct sockaddr_in serveraddr;
    //Connect to FNS
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(FNS_IP);
    serveraddr.sin_port = htons(FNS_PORT);
    FNS_sfd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(connect(FNS_sfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr)) == -1){
        perror("FileNameServer");
        return false;
    }
    else{
        printf("Connected to FileNameServer at %s:%d\n",FNS_IP,FNS_PORT);
    }
    char errmsg[50];
    for(int i=0;i<num_FDS;i++){
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = inet_addr(FDS_IP[i]);
        serveraddr.sin_port = htons(FDS_PORT[i]);
        FDS_sfd[i] = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP);
        if(connect(FDS_sfd[i],(struct sockaddr *)&serveraddr,sizeof(serveraddr)) == -1){
            sprintf(errmsg,"connect %s:%d ",FDS_IP[i],FDS_PORT[i]);
            perror(errmsg);
            return false;
        }
        else{
            printf(".");
        }
    }
    return true;
}

/*
 * Split a file into n parts and return the base file path
 * Suppose base file path is "myfile" and n = 3, then,
 * parts will be at "myfile_1" , "myfile_2" , "tmp_myfile_3"
 */
char *splitFile(char *filePath, int n){
    int rfd = open(filePath,O_RDONLY);
    if(rfd == -1){
        perror("open");
        return NULL;
    }
    int wfd,i;
    long sz;
    long CHUNK_SIZE = (getFileSize(filePath) / (long)n) + ((getFileSize(filePath) % (long)n) ? 1L : 0L);
    char *basename = allocmem(20);
    strcat(basename,randname(10));
    for(i=1;i<=n;i++){
        char buf[MAX_ARRAY_SIZE];
        sprintf(basename,"%.10s_%d",basename,i);
        long toRead = CHUNK_SIZE;
        wfd = open(basename,O_WRONLY|O_CREAT,0666);
        while(toRead > 0){
            sz = read(rfd,buf,MAX_ARRAY_SIZE);
            if(sz == 0)
                break;
            write(wfd,buf,sz);
            toRead -= sz;
        }
        close(wfd);
    }
    basename[10] = '\0';
    close(rfd);
    return basename;
}

char *joinFile(char *baseFilePath, int n){
    int wfd = open(baseFilePath,O_WRONLY|O_CREAT,0666);
    if(wfd == -1){
        perror("open");
        return NULL;
    }
    int rfd,i;
    long sz,CHUNK_SIZE;
    char *filePath = allocmem(31);
    for(i=1;i<=n;i++){
        sprintf(filePath,"%s_%d",baseFilePath,i);
        CHUNK_SIZE = getFileSize(filePath);
        if(CHUNK_SIZE == -1){
            perror("open");
            return NULL;
        }
        long toRead = CHUNK_SIZE;
        char buf[MAX_ARRAY_SIZE];
        rfd = open(filePath,O_RDONLY);
        if(rfd == -1){
            perror("open");
            return NULL;
        }
        while(toRead > 0){
            sz = read(rfd,buf,MAX_ARRAY_SIZE);
            write(wfd,buf,sz);
            toRead -= sz;
        }
        close(rfd);
    }
    close(wfd);
    free(filePath);
    return baseFilePath;
}


char *uploadPart(char *fpath,int FDS_idx,char *name){
    // printf("fpath - %s\n",fpath );
    long size = getFileSize(fpath);
    if(size == -1)
        return NULL;
    int fd = open(fpath,O_RDONLY);
    char *buf = allocmem(51);
    sprintf(buf,"UPL %ld %s",size, name);
    if(write(FDS_sfd[FDS_idx],buf,strlen(buf))==0)
        return NULL;
    //server returns a string
    int sz = read(FDS_sfd[FDS_idx],buf,50);  //ERROR or a Unique ID
    if(sz == 0)
        return NULL;
    buf[sz] = '\0';
    if(strcmp(buf,"ERROR")==0){
        perror("FDS returned ERROR.");
        return NULL;
    }

    long ret = sendfile(FDS_sfd[FDS_idx],fd,0,size);
    if(ret == -1){
        perror("sendfile");
        return NULL;
    }
    if(ret != 0)
        printf("Uploaded %ld bytes of %s to Data Server %d\n",ret,buf,FDS_idx+1);
    close(fd);
    return buf;
}

int downloadPart(char *uid, char *tgtPath,int FDS_idx){
    long size;
    int sockfd = FDS_sfd[FDS_idx];
    char buf[51] = {0};
    sprintf(buf,"DWN %s",uid);
    if(write(sockfd,buf,strlen(buf))==0)
        return false;
    size = read(sockfd,buf,50);
    if(size == 0)
        return false;
    buf[size] = '\0';
    sscanf(buf,"%ld",&size);
    if(size == -1){
        printf("ERROR in downloading from Date server %d.\n",FDS_idx+1);
        return false;
    }
    strcpy(buf,"READY");
    if(write(sockfd,buf,strlen(buf))==0)
        return false;
    int wfd = open(tgtPath,O_WRONLY|O_CREAT,0666);
    char wbuf[TCP_BUFFER_SIZE] = {0};
    long remain_data = size, len;
    while ((remain_data > 0) && ((len = read(sockfd, wbuf, TCP_BUFFER_SIZE)) > 0))
    {
        write(wfd,wbuf,len);
        remain_data -= len;
    }
    if(size != 0)
        printf("Downloaded %ld bytes for %s from Data Server-%d\n",size,uid,FDS_idx+1);
    close(wfd);
    return true;
}


int removePart(char *uid, int FDS_idx){
    long size;
    int sockfd = FDS_sfd[FDS_idx];
    char buf[51] = {0};
    sprintf(buf,"REM %s",uid);
    if(write(sockfd,buf,strlen(buf)) == 0)
        return false;
    size = read(sockfd,buf,50);
    if(size == 0)
        return false;
    buf[size] = '\0';
    if(strcmp(buf,"DONE")==0){
        return true;
    }
    else{
        return false;
    }
}


int newFnsEntry(char *remote_path, char **uidarr){
    // printf("rp - %s\n",remote_path );
    int i=0;
    int size = 5;
    size += strlen(remote_path);
    while(uidarr[i] != NULL){
        size += strlen(uidarr[i]) + 1;
        i++;
    }
    char buf[size];
    strcpy(buf,remote_path);
    i = 0;
    while(uidarr[i] != NULL){
        strcat(buf,"#");
        strcat(buf,uidarr[i++]);
    }
    size = strlen(buf);
    char tmp[21];
    sprintf(tmp,"UPL %d",size);
    if(write(FNS_sfd,tmp,strlen(tmp))==0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    i = read(FNS_sfd,tmp,20);
    if(i==0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    tmp[i] = '\0';
    if(strcmp(tmp,"READY")==0){
        if(write(FNS_sfd,buf,size)==0)
        {
            perror("Connection to one or more servers lost.");
            exit(-1);
        }
        printf("Transfer of file complete\n\n");
        return true;
    }
    else{
        printf("Error in transferring file.\n");
        return false;
    }
}


char **getFnsData(char *remote_path){
    int size;
    char buf[TCP_BUFFER_SIZE];
    sprintf(buf,"DWN %s",remote_path);
    if(write(FNS_sfd,buf,strlen(buf)) == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    size = read(FNS_sfd,buf,TCP_BUFFER_SIZE);
    if(size == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    buf[size] = '\0';
    if(size == 2 && strcmp(buf,"-1")==0){
        printf("ERROR File Not Found.\n");
        return NULL;
    }
    char **uidarr = strSplit(buf,'#');
    return uidarr;
}


char **getLs(char *remote_path){
    int size;
    char buf[TCP_BUFFER_SIZE];
    sprintf(buf,"LS %s",remote_path);
    if(write(FNS_sfd,buf,strlen(buf)) == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    size = read(FNS_sfd,buf,TCP_BUFFER_SIZE);
    if(size == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    buf[size] = '\0';
    if(size == 2 && strcmp(buf,"-1")==0){
        printf("ERROR Directory Not Found.\n");
        return NULL;
    }
    char *tbuf = trim(buf);
    char **lsarr = strSplit(tbuf,'\n');
    return lsarr;
}


int removeFnsEntry(char *remote_path){
    long size = strlen(remote_path) + 10;
    char buf[size];
    sprintf(buf,"REM %s",remote_path);
    if(write(FNS_sfd,buf,strlen(buf))==0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    size = read(FNS_sfd,buf,50);
    if(size == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    buf[size] = '\0';
    if(strcmp(buf,"DONE")==0){
        printf("Successfully removed %s.\n",remote_path);
        return true;
    }
    else{
        printf("Error in removing %s.\n",remote_path);
        return false;
    }
}


int moveFnsEntry(char *srcPath, char *destPath){
    long size = strlen(srcPath) + strlen(destPath) + 10;
    char buf[size];
    sprintf(buf,"MOV %s %s",srcPath,destPath);
    if(write(FNS_sfd,buf,strlen(buf))==0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    size = read(FNS_sfd,buf,50);
    if(size == 0)
    {
        perror("Connection to one or more servers lost.");
        exit(-1);
    }
    buf[size] = '\0';
    if(strcmp(buf,"DONE")==0){
        printf("Successfully moved.\n");
        return true;
    }
    else{
        printf("Error in moving.\n");
        return false;
    }
}

int cmd_rm(char *fpath, int isRecursive);

int uploadFile(char *rpath, char *fpath){
    if(rpath == NULL || fpath == NULL)
        return false;
    char *basepath = splitFile(fpath,num_FDS);  //basepath has max size of 30
    if(basepath == NULL)
        return false;
    // printf("rpath - %s\n",rpath );
    // printf("fpath - %s\n",fpath );
    // printf("basepath - %s\n",basepath );
    int sz = strlen(basepath) + 10;
    char ppath[sz]; //part Path
    int i;
    char **uidarr = (char **)(calloc(num_FDS+1,sizeof(char *)));
    uidarr[num_FDS] = NULL;
    int p[num_FDS][2];
    int cpid[num_FDS];
    for(i=0;i<num_FDS;i++){
        pipe(p[i]);
        if((cpid[i] = fork()) == 0){
            close(p[i][0]);
            sprintf(ppath,"%s_%d",basepath,i+1);

            int var = 0;
            for(var=strlen(fpath)-1;var>=0;var--)
            {
                if(fpath[var] == '/')
                    break;
            }
            if(var!=0)
                var++;

            char *tuid = uploadPart(ppath,i,fpath+var);
            if(tuid == NULL){
                perror("UploadPart has returned null");
                char cbuf[30] = {0};
                strcpy(cbuf,"null");
                write(p[i][1],cbuf,4);
                exit(-1);
            }
            else{
                write(p[i][1],tuid,strlen(tuid));
                exit(0);
            }
        }
        close(p[i][1]);
    }
    int partsDone = 0;
    char buf[101] = {0};
    for(i=0;i<num_FDS;i = (i+1)%num_FDS){
        if(partsDone == num_FDS)
            break;
        if(cpid[i] == -1)
            continue;
        int sz = read(p[i][0],buf,100);
        buf[sz] = '\0';
        if(sz == 4 && strcmp(buf,"null")==0){
            return false;
        }
        uidarr[i] = allocmem(sz+1);
        strcpy(uidarr[i],buf);
        waitpid(cpid[i],NULL,0);
        partsDone++;
        cpid[i] = -1;

        // printf("%s\n",uidarr[i] );
    }
    int failed = false;
    if(rpath[strlen(rpath)-1] == '/')
        strcat(rpath,fpath);
    if(!newFnsEntry(rpath,uidarr)){
        perror("Failed while trying to add entry to FNS.");
        i = 0;
        while(uidarr[i] != NULL){
            removePart(uidarr[i],i);
            i++;
        }
        failed = true;
    }
    //remove all temporarily generated files
    for(i=0;i<num_FDS;i++){
        sprintf(ppath,"%s_%d",basepath,i+1);
        cmd_rm(ppath,false);
    }
    if(failed)
        return false;
    return true;

}


char *downloadFile(char *rpath){
    char **uidarr = getFnsData(rpath);
    if(uidarr == NULL)
        return NULL;
    int j;
    for(int i=0;i<strlen(rpath);i++)
        if(rpath[i]=='/')
            j=i;
    char *basepath = allocmem(30);
    strcat(basepath,rpath+j+1);
    int i;
    int cpid[num_FDS];
    for(i=0;i<num_FDS;i++){
        if((cpid[i] = fork()) == 0) {
            sprintf(basepath, "%s_%d", basepath, i + 1);
            if (downloadPart(uidarr[i], basepath, i))
                exit(0);
            else {
                perror("Download Part failed.");
                perror(uidarr[i]);
                exit(1);
            }
        }
    }
    int partsDone = 0;
    for(i=0;i<num_FDS;i = (i+1)%num_FDS){
        if(partsDone == num_FDS)
            break;
        if(cpid[i] == -1)
            continue;
        int status;
        waitpid(cpid[i],&status,0);
        if(WEXITSTATUS(status) == 1){
            return NULL;
        }
        partsDone++;
        cpid[i] = -1;
    }
    if((basepath = joinFile(basepath,num_FDS)) == NULL){
        perror("Error joining parts.");
    }
    //remove the temporarily generated parts
    for(i=0;i<num_FDS;i++){
        char tmppath[50] = {0};
        sprintf(tmppath, "%s_%d", basepath, i + 1); 
        cmd_rm(tmppath,false);
    }
    return basepath;
}

char **getLsLocal(char *dpath){
    int p[2];
    pipe(p);
    if(fork() == 0){
        close(p[0]);
        close(1);
        dup2(p[1],1);
        execlp("ls","ls","-F",dpath,NULL);
        perror("execlp");
        exit(0);
    }
    close(p[1]);
    wait(NULL);
    char buf[TCP_BUFFER_SIZE] = {0};
    int nr = read(p[0],buf,TCP_BUFFER_SIZE);
    buf[nr] = '\0';
    close(p[0]);
    return strSplit(trim(buf),'\n');
}

int removeFile(char *rpath){
    char **uidarr = getFnsData(rpath);
    if(uidarr == NULL)
        return false;
    int i;
    for(i=0;i<num_FDS;i++){
        removePart(uidarr[i],i);
    }
    removeFnsEntry(rpath);
    return true;
}

int cmd_ls(char *dpath){
    char **larr;
    if(!inBigfs(dpath))
        larr = getLsLocal(dpath);
    else
        larr = getLs(dpath);
    if(larr == NULL)
        return false;
    int i = 0;
    while(larr[i] != NULL){
        printf("%s\n",larr[i++]);
    }
    return true;
}

int cmd_rm(char *fpath, int isRecursive){
    if(!inBigfs(fpath)){
        //rm in local storage
        if(fork() == 0){
            execlp("rm","rm","-r",fpath,NULL);
            perror("execlp");
            exit(0);
        }
        wait(NULL);
        return true;
    }
    if(!isRecursive)
        return removeFile(fpath);
    else{
        //remove a directory
        char **larr = getLs(fpath);
        int i=0;
        while(larr[i] != NULL){
            int n = strlen(larr[i]);
            char *nfpath = allocmem(strlen(fpath) + n+1);
            strcpy(nfpath,fpath);
            strcat(nfpath,larr[i]);
            if(larr[i][n-1] == '/'){
                //it is a directory
                cmd_rm(nfpath,true);
            }
            else{
                //it is a file
                cmd_rm(nfpath,false);
            }
            free(nfpath);
            i++;
        }
        removeFnsEntry(fpath);
        return true;
    }
}

int cmd_mv(char *arg1, char *arg2){
    if(!inBigfs(arg1) && !inBigfs(arg2)){
        //mv in local storage
        if(fork() == 0){
            execlp("mv","mv",arg1,arg2,NULL);
            perror("execlp");
            exit(0);
        }
        wait(NULL);
        return true;
    }
    if(inBigfs(arg1) && inBigfs(arg2)){
        if(moveFnsEntry(arg1,arg2))
            return true;
        else
            return false;
    }
    else{
        printf("Not implemented\n");
        return false;
    }
}


int cmd_cp(char *arg1, char *arg2, int isRecursive){
    if(!inBigfs(arg1) && !inBigfs(arg2)){
        //cp in local storage (both files are local)
        if(fork() == 0){
            if(isRecursive)
                execlp("cp","cp","-r",arg1,arg2,NULL);
            else
                execlp("cp","cp",arg1,arg2,NULL);
            perror("execlp");
            exit(0);
        }
        wait(NULL);
        return true;
    }
    if(inBigfs(arg1) && inBigfs(arg2)){
        //both remote
        //TODO: WRITE CODE FOR THIS PART
        return false;
    }
    if(!isRecursive){
        //copy a single file
        if(inBigfs(arg1)) {
            //download remote file to tmpPath
            char *tmpPath = downloadFile(arg1);
            if (tmpPath == NULL)
                return false;
            //move it to the required location
            if (!cmd_mv(tmpPath, arg2)) {
                //remove the file, if move fails and report the error
                cmd_rm(tmpPath,false);
                return false;
            }
            return true;
        }
        else{
            //upload file to bigfs
            if(uploadFile(arg2,arg1))
                return true;
            else{
                perror("Upload file failed.");
                return false;
            }
        }
    }
    else{
        //copy a directory
        if(inBigfs(arg1))
            if (!makeDir(arg2))
                return false;
        int arg1len = strlen(arg1);
        if (arg1[arg1len - 1] != '/') {
            //this folder along with its content will be copied
            int n = strlen(arg1);
            int slc = 0;

            int i;
            for(i=0;i<strlen(arg1);i++)
                if(arg1[i] == '/')
                    slc++;

            i=0;
            while(slc){
                if(arg1[i] == '/')
                    slc--;
                i++;
            }
            char *ddir = (arg1 + i);


            char *narg1 = allocmem(arg1len + 5);
            char *narg2 = allocmem(strlen(arg2) + strlen(ddir) + 5);
            strcpy(narg1, arg1);
            strcat(narg1, "/");
            strcpy(narg2, arg2);
            strcat(narg2, ddir);
            strcat(narg2, "/");
            return cmd_cp(narg1, narg2, true);
        }
        char **larr;
        if(inBigfs(arg1)) {
            //download recursively
            larr = getLs(arg1);
        }
        else{
           larr = getLsLocal(arg1);
        }
        int i = 0;
        while (larr[i] != NULL) {
            int n = strlen(larr[i]);
            char *narg1 = allocmem(strlen(arg1) + n + 1);
            strcpy(narg1, arg1);
            strcat(narg1, larr[i]);
            char *narg2 = allocmem(strlen(arg2) + n + 1);
            strcpy(narg2, arg2);
            strcat(narg2, larr[i]);
            if (larr[i][n - 1] == '/') {
                //it is a directory
                cmd_cp(narg1, narg2, true);
            } else {
                //it is a file
                cmd_cp(narg1, narg2, false);
            }
            free(narg1);
            free(narg2);
            i++;
        }
        return true;
    }
}


int cmd_cat(char *fpath){
    if(fpath == NULL)
        return false;
    char *localpath = fpath;
    if(inBigfs(fpath)){
        //cat in bigfs
        localpath = downloadFile(fpath);
        if(localpath == NULL)
            return false;
    }
    //cat in local storage
    if(fork() == 0){
        execlp("cat","cat",localpath,NULL);
        perror("execlp");
        exit(0);
    }
    wait(NULL);
    //remove the temporarily downloaded file
    if(inBigfs(fpath)){
        cmd_rm(localpath,false);
        free(localpath);
    }
    return true;
}

void execute_cmd(char *cmd){
    //remove leading and trailing spaces from the cmd
    cmd = trim(cmd);
    //split the cmd based on spaces
    char **cmdarr = strSplit(cmd,' ');
    if(cmdarr == NULL)
        return;
    if(strcmp(cmdarr[0],"cat")==0){
        if(cmdarr[1] && !cmdarr[2] && cmd_cat(cmdarr[1]))
            return;
        else
            printf("Please check the file path.\n");
    }
    else if(strcmp(cmdarr[0],"cp")==0){
        if(cmdarr[1] && cmdarr[2] && cmdarr[3] && !cmdarr[4] && (strcmp(cmdarr[1],"-r")==0) && cmd_cp(cmdarr[2],cmdarr[3],true))
            return;
        else if(cmdarr[1] && cmdarr[2] && !cmdarr[3] && cmd_cp(cmdarr[1],cmdarr[2],false))
            return;
        else
            printf("Please check the entered paths.\n");
    }
    else if(strcmp(cmdarr[0],"mv")==0){
        if(cmdarr[1] && cmdarr[2] && !cmdarr[3] && cmd_mv(cmdarr[1],cmdarr[2]))
            return;
        else
            printf("Please check the entered paths.\n");
    }
    else if(strcmp(cmdarr[0],"ls")==0){
        if(cmdarr[1] && !cmdarr[2] && cmd_ls(cmdarr[1]))
            return;
        else if(!cmdarr[1] && cmd_ls("bigfs"))
            return;
        else
            printf("There was an error while trying to do ls.\n");
    }
    else if(strcmp(cmdarr[0],"rm")==0){
        if(cmdarr[1] && cmdarr[2] && !cmdarr[3] && (strcmp(cmdarr[1],"-r")==0) && cmd_rm(cmdarr[2],true))
            return;
        else if(cmdarr[1] && !cmdarr[2] && cmd_rm(cmdarr[1],false))
            return;
        else
            printf("Unable to delete the given file/folder.\n");
    }
    else if(strcmp(cmdarr[0],"exit")==0){
        printf("Exiting...\n");
        exit(0);
    }
    else{
        printf("Command not recognized.\n");
        return;
    }
}



int main(int argc, char *argv[]) {
    if(argc != 2){
        printf("Give path to the config file as a command line argument.\n");
        return -1;
    }
    char *cfgfile = argv[1];
    srand(time(0));
    //make a directory to store temporary files
    makeDir("tmp");
    //read the config file
    if(!readConfig(cfgfile))
    {
        perror("Error reading the config file.");
        printf("\n## CONFIG FILE FORMAT ##\n\n");
        printf("<FNS IP> <FNS PORT>\n");
        printf("<k = Number of FDS>\n");
        printf("<FDS1 IP> <FDS1 PORT>\n");
        printf("<FDS2 IP> <FDS2 PORT>\n");
        printf("..\n");
        printf("..\n");
        printf("..\n");
        printf("<FDSk IP> <FDSk PORT>\n\n");
        return -1;
    }
    //create connection with all the servers
    if(!createConnection()){
        perror("[ERROR]: Problem connecting with server(s).");
        return -1;
    }
    char inp[101] = {0};

    while(1){
        printf("\n $ ");
        fgets(inp,100,stdin);
        execute_cmd(inp);
    }

    return 0;
}
#include<stdio.h> 
#include<stdlib.h>
#include<unistd.h>
#include<string.h> 
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/ipc.h>
#include<sys/msg.h>
#include<signal.h> 

struct msgbuf {
   long mtype;
   char mtext[2];
};

int flagSTOP=0;
int flagEND=0;
int flagMES=0;
int flagSEND=0;

key_t key;
int msgid;
int pid[3];
int fd1[2];
int fd2[2];
int fd3[2];
struct msgbuf msg;

void sendMSG() {
    for(int i = 0; i<3; i++){
        if(pid[i] != getpid()){
            kill(pid[i], SIGUSR1);
            msg.mtype = pid[i];
            sprintf(msg.mtext,"%d%d", flagSTOP, flagEND);
            msgsnd(msgid, &msg, sizeof(msg), 0);
        }
    }

    flagSEND = 0;
}

void rcvMSG() {
    msgrcv(msgid, &msg, sizeof(msg), getpid(), 0);
    char help[1];
    help[0] = msg.mtext[0];
    flagSTOP = atoi(help);
    help[0] = msg.mtext[1];
    flagEND = atoi(help);

    if(flagEND) {
        raise(SIGUSR2);
    }
}

void handleSTOP(){
    printf("Wstrzymanie pracy\n");
    flagSTOP=1;
    sendMSG();
}

void handleCONT(){
    printf("Kontynuacja pracy\n");
    flagSTOP=0;
    sendMSG();
}

void handleMES(){
    rcvMSG();
}

void handleEND(){
    flagEND=1;
    sendMSG();
    close(fd1[0]);
    close(fd1[1]);
    close(fd2[0]);
    close(fd2[1]);
    close(fd3[0]);
    close(fd3[1]);
    exit(0);
}

void firstProcess(){
    char s[100];
    memset(s,0,100);
    fgets(s,100,stdin);
    //scanf("%s", &s);
    write(fd1[1], s, strlen(s)+1);
    while(flagSTOP)
        pause();
}

void secondProcess(){
    char s[100];
    memset(s,0,100);
    read(fd1[0], s, 100);
    
    while(flagSTOP){
        pause();
        //usleep(100);
    }

    int i=0, flag=1;
    if((int)s[0]==43 || (int)s[strlen(s)-2]==43){
        flag=0;
    }
    
    for(i=0;i<strlen(s)-1 && flag != 0;i++) {
        if(((int)s[i]>57 || (int)s[i]<48) && (int)s[i]!=43) {
            flag=0;                    
        }
    }
    if(flag == 1) {
        write(fd2[1], s, strlen(s)+1); 
    } else {
        write(fd2[1], "@", 2);
    }
}

void thirdProcess(){
    char s[100];
    memset(s,0,100);
    char number[9];
    memset(number, 0, 9);
    int i = 0, j = 0;
    int sum = 0;
    read(fd2[0], s, 100);

    while(flagSTOP){
        pause();
        //usleep(100);
    }
    if(flagEND){
        return;
    }

    if (s[0] == '@') {
        printf("Wprowadzono bledne dane\n");
        fflush(stdout);
    }
    else {
        for (i = 0; i <= strlen(s); i++) {
            if ((int)s[i] >= 48 && (int)s[i] <= 57) {
                number[j] = s[i];
                j++;
            }
            else {
                sum += atoi(number);
                j = 0;
                memset(number, 0, 9);
            }
        }
        printf("= %d\n", sum);
        fflush(stdout);
    }
}

void sendPID(int recvPID){
    msg.mtype = recvPID;
    int i = 0;
    for(i=0; i<3; i++){
        sprintf(msg.mtext,"%d", pid[i]);
        msgsnd(msgid, &msg, sizeof(msg), 0);
    }
}

void getPID(){
    int i = 0;
    for(i=0; i<3; i++){
        msgrcv(msgid, &msg, sizeof(msg), getpid(), 0);
        pid[i] = atoi(msg.mtext);
    }
}

int main(){
    
    signal(SIGUSR2, handleEND); 
    signal(SIGALRM, handleSTOP); 
    signal(SIGCONT, handleCONT); 
    signal(SIGUSR1, handleMES); 

    printf("Sygnaly : \n");
    printf("12 - koniec pracy\n");
    printf("14 - wstrzymaj komunikacje\n");
    printf("18 - kontynuacja komunikacji\n");
    printf("10 - komunikacja miedzy procesowa\n");

    key = ftok("key", 65); ; 
    msgid = msgget(key, 0666 | IPC_CREAT);

    if(pipe(fd1) == -1){
        printf("1 pipe error");
    }
    if(pipe(fd2) == -1){
        printf("2 pipe error");
    }
    if(pipe(fd3) == -1){
        printf("3 pipe error");
    }

    pid[0] = fork();
    if(pid[0] == 0) {
        getPID();
        close(fd1[0]);
        while(!flagEND){
            firstProcess();
        }
        close(fd1[1]);
        return 0;
    } else if (pid[0] < 0) 
        printf("1 fork error");
    

    pid[1] = fork();
    if(pid[1] == 0) {
        getPID();
        close(fd1[1]);
        close(fd2[0]);
        while(!flagEND){
            secondProcess();
        }
        close(fd1[0]);
        close(fd2[1]);
        return 0;
    } else if (pid[1] < 0) 
        printf("2 fork error");
    

    pid[2] = fork();
    if(pid[2] == 0) {
        pid[2] = getpid();
        sendPID(pid[0]);
        sendPID(pid[1]);
        close(fd2[1]);
        while(!flagEND) {
            thirdProcess();
        }
        return 0;
        close(fd2[0]);
    } else if (pid[2] < 0) 
        printf("3 fork error");
    


    printf("pid1 : %d\n", pid[0]);
    printf("pid2 : %d\n", pid[1]);
    printf("pid3 : %d\n", pid[2]);

    int status[3];
    while( !waitpid(pid[0], &status[0], WNOHANG) || !waitpid(pid[1], &status[1], WNOHANG) || !waitpid(pid[2], &status[2], WNOHANG)){
        sleep(1);
    }

    msgctl(msgid, IPC_RMID, NULL); 
    close(fd1[0]);
    close(fd1[1]);
    close(fd2[0]);
    close(fd2[1]);
    close(fd3[0]);
    close(fd3[1]);
}
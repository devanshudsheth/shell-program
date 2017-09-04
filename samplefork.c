#include <sys/signal.h>
#include <stdio.h>
int write(int,char*,int);
int read(int,char*,int);
int fork(void);
int kill(int,int);
int getpid(void);
int getppid(void);
int sleep(int);
int wait(int*);

//connect to cs1.utdallas.edu using the putty program or ssh
//save this file as samplefork.c, compile using command cc samplefork.c, execute using a.out

int main()
{
        int pid,i,j,stat,ppid;
        char c;

        write(1,"prog starts\n Please type a char and press ENTER\n",48);
	printf("my id (original process's id= %d\n",getpid());

        if ((pid=fork())==0) { 
                // child code
                write(1,"Child executing\n",16);
                printf("Child: My id is % d; Parent's pid = %d\n",getpid(),getppid());
                for(i=0;i<150000;i++){
		{sleep(1); write(1,"Child ",6);} // keep writing till killed by parent 
		}}

        else {
                // parent code 
                write(1,"parent executing\n",17);
                write(1,"Parent:Really want to kill ?",28);
		// wait for user to type something and enter key before we go about killing the child 
	        read(0,&c,1);   
		//send SIGKILL to the process with id = pid, which is the child 
                i=kill(pid,SIGKILL);
		//kill() is a system call used to send a signal to a process 
		printf("Parent:value of i is %d\n",i);
		// wait for child to terminate 
		wait(&stat);
		printf("Parent:Value of stat is %d\n",stat);
		// sending signal to a terminted process; should get return value of -1 (see man pages of kill()) 
                i=kill(pid,SIGINT);
		printf("Parent:value of i is %d\n",i);
                write(1,"parent terminating\n",19);
                }

}
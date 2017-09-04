#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#define DELIMITERS " \t\r\n\a"

int execute_command(char cmd1[],char *args1[])
{
        printf("Type in exit for exiting else type anything else");
        if (strcmp(cmd1,"exit") == 0)
        {
        exit(0);
        }
        if (cmd1 == NULL)
        {
        return 1;
        }
        else
        {
        int status;
        pid_t childPID;
        childPID = fork();
        if(childPID<0)
                {
                perror("fork() error");
                exit(-1);
                }
        if(childPID != 0)
                {
                printf("I am parent with PID: %d and my child is %d",getpid(), childPID);
                wait(NULL);
                }
        else
        {
        printf("I am the child with PID: %d and my parent is %d", getpid(), getppid());
        return execvp(cmd1,args1 );

                        perror("incorrect");
                        exit(-1);
                }
        }
        }
int main(int argc, char *argv)
{
char command[20];
int status;
do
{
printf("Enter command or enter \"exit \" to exit the shell:\n");
fgets(command, 20, stdin );
char cmd1[50] ;
strcpy(cmd1, command);
char *args1[] = { "command", NULL};
execute_command(cmd1, args1);
//free(*argv);
printf("%d", &argc);
}while(status);

return 0;
}
             
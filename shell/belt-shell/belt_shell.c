#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

char items[10][100];
int count=0;
volatile sig_atomic_t stop=0;

void handler(int sig)
{
    stop=1;
}

int main()
{
    char line[1024];
    char *args[20];

    struct sigaction sa;
    sa.sa_handler=handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGINT,&sa,NULL);

    while(1)
    {
        printf("belt-control$ ");
        fflush(stdout);

        if(fgets(line,sizeof(line),stdin)==NULL)
        {
            if(stop)
            {
                stop=0;
                count=0;
                printf("\n[ALERT] Emergency stop triggered, item queue cleared\n");
                clearerr(stdin);
                continue;
            }

            break;
        }

        if(stop)
        {
            stop=0;
            count=0;
            printf("\n[ALERT] Emergency stop triggered, item queue cleared\n");
            continue;
        }

        line[strcspn(line,"\n")]='\0';

        if(strlen(line)==0)
            continue;

        int argc=0;

        char *token=strtok(line," \t");

        while(token!=NULL && argc<19)
        {
            args[argc]=token;
            argc++;
            token=strtok(NULL," \t");
        }

        args[argc]=NULL;

        if(strcmp(args[0],"add_item")==0)
        {
            if(argc<2)
            {
                printf("Error: item name missing\n");
            }
            else if(count>=10)
            {
                printf("Error: item queue is full\n");
            }
            else
            {
                strcpy(items[count],args[1]);
                count++;
            }
        }
        else if(strcmp(args[0],"list_items")==0)
        {
            if(count==0)
            {
                printf("Queue is empty\n");
            }
            else
            {
                for(int i=0;i<count;i++)
                    printf("%s\n",items[i]);
            }
        }
        else if(strcmp(args[0],"quit")==0)
        {
            break;
        }
        else if(strcmp(args[0],"date")==0)
        {
            pid_t pid=fork();

            if(pid==0)
            {
                execlp("date","date",NULL);
                perror("date");
                exit(1);
            }
            else if(pid>0)
            {
                while(wait(NULL)==-1 && errno==EINTR);

                if(stop)
                {
                    stop=0;
                    count=0;
                    printf("\n[ALERT] Emergency stop triggered, item queue cleared\n");
                }
            }
            else
            {
                perror("fork");
            }
        }
        else if(strcmp(args[0],"ping")==0)
        {
            if(argc<2)
            {
                printf("Error: address missing\n");
            }
            else
            {
                pid_t pid=fork();

                if(pid==0)
                {
                    execlp("ping","ping","-c","4",args[1],NULL);
                    perror("ping");
                    exit(1);
                }
                else if(pid>0)
                {
                    while(wait(NULL)==-1 && errno==EINTR);

                    if(stop)
                    {
                        stop=0;
                        count=0;
                        printf("\n[ALERT] Emergency stop triggered, item queue cleared\n");
                    }
                }
                else
                {
                    perror("fork");
                }
            }
        }
        else
        {
            printf("Error: command not found\n");
        }
    }

    return 0;
}

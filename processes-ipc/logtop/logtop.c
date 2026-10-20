#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(int argc,char *argv[])
{
    if(argc!=3)
    {
        printf("usage:%s <logfile> <column>\n",argv[0]);
        return 1;
    }

    char *file=argv[1];
    int col=atoi(argv[2]);

    if(col<1)
    {
        printf("column should be at least 1\n");
        return 1;
    }

    char column[20];
    sprintf(column,"%d",col);

    char *child1[]={"tr","-s"," \t"," ",NULL};
    char *child2[]={"cut","-d"," ","-f",column,NULL};
    char *child3[]={"sort",NULL};
    char *child4[]={"uniq","-c",NULL};
    char *child5[]={"sort","-rn",NULL};
    char *child6[]={"head","-5",NULL};

    char **child[]={child1,child2,child3,child4,child5,child6};

    int pipes[5][2];
    pid_t pids[6];

    for(int i=0;i<5;i++)
    {
        if(pipe(pipes[i])==-1)
        {
            perror("pipe");
            return 1;
        }
    }

    for(int i=0;i<6;i++)
    {
        pids[i]=fork();

        if(pids[i]==-1)
        {
            perror("fork");
            return 1;
        }

        if(pids[i]==0)
        {
            if(i==0)
            {
                int fd=open(file,O_RDONLY);

                if(fd==-1)
                {
                    perror("open");
                    exit(1);
                }

                dup2(fd,STDIN_FILENO);
                close(fd);
            }
            else
            {
                dup2(pipes[i-1][0],STDIN_FILENO);
            }

            if(i<5)
            {
                dup2(pipes[i][1],STDOUT_FILENO);
            }

            for(int j=0;j<5;j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            execvp(child[i][0],child[i]);

            perror("execvp");
            exit(1);
        }
    }

    for(int i=0;i<5;i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for(int i=0;i<6;i++)
    {
        waitpid(pids[i],NULL,0);
    }

    return 0;
}

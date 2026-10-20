#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>

struct msgbuf
{
    long type;
    long pid;
};

struct process
{
    long pid;
    char user[50];
    char command[200];
    double cpu;
    double mem;
    double score;
};

int compare(const void *a,const void *b)
{
    struct process *p1=(struct process *)a;
    struct process *p2=(struct process *)b;

    if(p1->score < p2->score)
        return 1;

    if(p1->score > p2->score)
        return -1;

    return 0;
}

int getprocess(struct process p[])
{
    FILE *fp;
    char line[1000];
    int count=0;

    fp=popen("ps aux","r");

    if(fp==NULL)
        return 0;

    fgets(line,sizeof(line),fp);

    while(fgets(line,sizeof(line),fp)!=NULL && count<1024)
    {
        char user[50];
        long pid;
        double cpu,mem;
        long vsz,rss;
        char tty[30],stat[30],start[30],time[30];
        int matched;
        int used;

        matched=sscanf(line,"%49s %ld %lf %lf %ld %ld %29s %29s %29s %29s %n",
                       user,&pid,&cpu,&mem,&vsz,&rss,
                       tty,stat,start,time,&used);

        if(matched<10)
            continue;

        p[count].pid=pid;
        strcpy(p[count].user,user);
        p[count].cpu=cpu;
        p[count].mem=mem;

        char *c=line+used;

        while(*c==' ')
            c++;

        c[strcspn(c,"\n")]='\0';

        strncpy(p[count].command,c,sizeof(p[count].command)-1);
        p[count].command[sizeof(p[count].command)-1]='\0';

        p[count].score=3*p[count].cpu+2*p[count].mem;

        count++;
    }

    pclose(fp);

    return count;
}

void printprocess(int k)
{
    struct process p[1024];

    int count=getprocess(p);

    qsort(p,count,sizeof(struct process),compare);

    printf("\nPID\tCOMMAND\t\t\tCPU%%\tMEM%%\tSCORE\n");

    int x;

    if(k<count)
        x=k;
    else
        x=count;

    for(int i=0;i<x;i++)
    {
        printf("%ld\t%-25.25s\t%.2f\t%.2f\t%.2f\n",
               p[i].pid,
               p[i].command,
               p[i].cpu,
               p[i].mem,
               p[i].score);
    }

    fflush(stdout);
}

void childwork(int q,int n,int k,int r)
{
    int i=0;

    while(1)
    {
        printprocess(k);

        i++;

        sleep(n);

        if(i%r==0)
        {
            struct msgbuf m;

            if(msgrcv(q,&m,sizeof(m.pid),1,0)==-1)
                break;

            if(m.pid==-2)
            {
                msgctl(q,IPC_RMID,NULL);
                break;
            }

            if(m.pid==-1)
                continue;

            struct process p[1024];

            int count=getprocess(p);
            int found=0;

            for(int j=0;j<count;j++)
            {
                if(p[j].pid==m.pid)
                {
                    printf("\nProcess information:\n");
                    printf("PID: %ld\n",p[j].pid);
                    printf("COMMAND: %s\n",p[j].command);
                    printf("OWNER: %s\n",p[j].user);
                    printf("CPU%%: %.2f\n",p[j].cpu);
                    printf("MEM%%: %.2f\n",p[j].mem);
                    printf("SCORE: %.2f\n",p[j].score);

                    fflush(stdout);

                    if(kill(p[j].pid,SIGKILL)==0)
                        printf("Process %ld killed.\n",p[j].pid);
                    else
                        perror("kill");

                    fflush(stdout);

                    found=1;
                    break;
                }
            }

            if(found==0)
                printf("Process %ld not found.\n",m.pid);

            fflush(stdout);
        }
    }
}

void parentwork(int q,int n,int r)
{
    while(1)
    {
        sleep(n*r);

        long x;

        printf("\nEnter PID (-1 to skip, -2 to quit): ");
        fflush(stdout);

        if(scanf("%ld",&x)!=1)
        {
            printf("Invalid input\n");

            while(getchar()!='\n');

            continue;
        }

        struct msgbuf m;

        m.type=1;
        m.pid=x;

        if(msgsnd(q,&m,sizeof(m.pid),0)==-1)
        {
            perror("msgsnd");
            break;
        }

        if(x==-2)
        {
            wait(NULL);
            break;
        }
    }
}

int main()
{
    int n,k,r;

    printf("Enter n: ");
    scanf("%d",&n);

    printf("Enter k: ");
    scanf("%d",&k);

    printf("Enter r: ");
    scanf("%d",&r);

    if(n<=0 || k<=0 || r<=0)
    {
        printf("Invalid input\n");
        return 1;
    }

    int q=msgget(IPC_PRIVATE,0666|IPC_CREAT);

    if(q==-1)
    {
        perror("msgget");
        return 1;
    }

    pid_t pid=fork();

    if(pid<0)
    {
        perror("fork");
        msgctl(q,IPC_RMID,NULL);
        return 1;
    }

    if(pid==0)
    {
        childwork(q,n,k,r);
        exit(0);
    }
    else
    {
        parentwork(q,n,r);
    }

    return 0;
}

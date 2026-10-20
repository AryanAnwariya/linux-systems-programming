#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

int gcd(int a,int b)
{
    while(b)
    {
        int t=a%b;
        a=b;
        b=t;
    }
    return a;
}

int main()
{
    int a[]={18, 24, 35, 49, 10, 63, 27, 40, 14, 21};

    int n=10;
    int rem=n;
    int p1[2],p2[2];
    int x,y,g,i,j,k;

    pipe(p1);
    pipe(p2);

    pid_t pid=fork();

    if(pid==0)
    {
        close(p1[1]);
        close(p2[0]);

        for(i=0;i<n/2;i++)
        {
            read(p1[0],&x,sizeof(x));
            read(p1[0],&y,sizeof(y));

            g=gcd(x,y);

            printf("Child: %d %d %d\n",x,y,g);
            fflush(stdout);

            usleep((time(NULL)%g)*1000);

            write(p2[1],&g,sizeof(g));
        }

        close(p1[0]);
        close(p2[1]);
    }
    else
    {
        close(p1[0]);
        close(p2[1]);

        srand(time(NULL));

        for(i=0;i<n/2;i++)
        {
            j=rand()%rem;
            x=a[j];

            a[j]=a[rem-1];
            rem--;

            k=rand()%rem;
            y=a[k];

            a[k]=a[rem-1];
            rem--;

            printf("Parent: %d %d\n",x,y);
            fflush(stdout);

            write(p1[1],&x,sizeof(x));
            write(p1[1],&y,sizeof(y));

            read(p2[0],&g,sizeof(g));

            printf("Parent: %d\n",g);
            fflush(stdout);

            usleep(g*1000);
        }

        close(p1[1]);
        close(p2[0]);

        wait(NULL);
    }

    return 0;
}

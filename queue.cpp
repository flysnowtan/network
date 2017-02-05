#include "tqueue.h"
#include "tepollsvrenum.h"
#include <iostream>
#include<sys/time.h>

using namespace std;


void *PushThread(void* arg)
{
    TRingQueue * queue = (TRingQueue*) arg;

    for(int i = 0; i < MAX_ARRAY_SIZE; i++)
    {   
        if(!queue->Push(i))
        {   
            i--;
            continue;
        }   
    }   
}

void *PopThread(void* arg)
{
    TRingQueue * queue = (TRingQueue*) arg;

    int data;
    while(true)
    {   
        if(queue->Pop(&data, false))
        {   
            sched_yield();
        }   
        else 
        {   
            return NULL;
        }   
    }   
}

int main(void)
{
    TRingQueue inQueue;
    cout <<"start!"<<endl;

    struct timeval start,end;
    gettimeofday(&start, NULL);
    pthread_t pushThread, popThread;
    pthread_create(&pushThread, NULL, PushThread, &inQueue);
    pthread_create(&popThread, NULL, PopThread, &inQueue);

    pthread_join(pushThread, NULL);
    pthread_join(popThread, NULL);
    gettimeofday(&end, NULL);

    cout <<"end! spend time: "<< (end.tv_sec-start.tv_sec)*1000 + (end.tv_usec - start.tv_usec)/1000 << " ms" << endl;

    return 0;
}

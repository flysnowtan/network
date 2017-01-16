#include <netinet/in.h>
#include <memory>
#include "tmultithreadsvr.h"
#include "tepollutils.h"
#include "tepoller.h"
#include "tqueue.h"
#include "tepollhandler.h"
#include <stdio.h>
#include <vector>
#include <errno.h>

//这些最好写到配置中
#define LISTEN_PORT 9911
#define IO_THREAD_NUM 8
#define WORKER_NUM_PER_IO_THREAD 4
#define EPOLL_WAIT_TIMEOUT 5


const bool bConnectionClose = false;
const bool bIsNeedWorker = true;


using namespace std;

struct AcptThreadArg
{
    vector<int> vecListenFd;
    TEpoller ** ppEpoller;
};

struct MainThreadArg
{
    TEpoller * pEpoller;
    TEpollHandler * pHandler;
};

struct IOThreadArg
{
    TEpoller * pEpoller;
    TQueue * pQueue;
    TEpollHandler * pHandler;
};

struct WorkerThreadArg
{
    TEpoller * pEpoller;
    TQueue *pQueue;
    TEpollHandler * pHandler;
};
static bool bIsMainRunning =false;

static void* RunWorkerThread(void* arg)
{
    struct WorkerThreadArg * workerArg = (struct WorkerThreadArg *)arg;
    TEpoller * pEpoller = workerArg->pEpoller;
    TEpollHandler * pHandler = workerArg->pHandler;
    TQueue * pQueue = workerArg->pQueue;

    int iFd = -1;
    TElement * pElm = NULL;
    while(bIsMainRunning && pQueue->Pop(&pElm) && pElm)
    {
        if(pElm->IsProcessing())
        {
            iFd = *(pElm->GetListenFd());
            pHandler->DoWork();
            if(pEpoller->ModifyFd(iFd, EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
            {
                pElm->UnSetProcessing();
            }
        }
    }

    delete workerArg;
    workerArg = NULL;
    return NULL;
}

static void* RunMainThread(void* arg)
{
    struct MainThreadArg *mainArg = (struct MainThreadArg *)arg;
    TEpoller * pEpoller = mainArg->pEpoller;
    TEpollHandler * pHandler = mainArg->pHandler;

    while(bIsMainRunning)
    {
        int iFd = -1;
        int iRet = 0;
        int iEvents = pEpoller->Wait(EPOLL_WAIT_TIMEOUT);

        for(int i = 0; i < iEvents; i++)
        {
            struct epoll_event *event = pEpoller->GetEvent(i);
            TElement * pElm = (TElement *)event->data.ptr;
            iFd = *(pElm->GetListenFd());
            void * pUserData = pElm->GetData();

            if(event->events & EPOLLIN)
            {
                iRet = pHandler->HandleRecvData(pElm, true);
                if(iRet == 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;
                    }
                }
                else if(iRet > 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;
                    }
                }
                else
                {
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    delete pElm;
                    pElm = NULL;
                }
            }
            else if(event->events & EPOLLOUT)
            {
                iRet = pHandler->HandleSendData(pElm);
                if(iRet == 0)
                {
                    if(bConnectionClose)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;

                    }
                    else
                    {
                        if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                        {
                            pEpoller->DelFd(iFd);
                            close(iFd);
                            delete pElm;
                            pElm = NULL;
                        }
                    }
                }
                else if(iRet < 0)
                {
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    delete pElm;
                    pElm = NULL;
                }
            }
            else
            {
                pEpoller->DelFd(iFd);
                close(iFd);
                delete pElm;
                pElm = NULL;
            }
        }
    }

    delete mainArg;
    mainArg = NULL;
    return NULL;
}

//data new by worker need to release!
static void* RunIOThread(void* arg)
{
    struct IOThreadArg *ioArg = (struct IOThreadArg *)arg;
    TEpoller *pEpoller = ioArg->pEpoller;
    TQueue * pQueue = ioArg->pQueue;
    TEpollHandler *pHandler = ioArg->pHandler;

    while(bIsMainRunning)
    {
        int iFd = -1;
        int iRet = 0;
        int iEvents = pEpoller->Wait(EPOLL_WAIT_TIMEOUT);
        for(int i = 0; i < iEvents; i++)
        {
            struct epoll_event *event = pEpoller->GetEvent(i);
            TElement * pElm = (TElement *)event->data.ptr;
            iFd = *(pElm->GetListenFd());
            void * pUserData = pElm->GetData();

            if(event->events & EPOLLIN)
            {
                if(pElm->IsProcessing())    continue;
                iRet = pHandler->HandleRecvData(pElm);
                if(iRet == 0)
                {
                    pElm->SetProcessing();
                    if(!pQueue->Push(pElm))
                    {
                        //队列满了就删掉有数据的fd，保护一下
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;
                    }
                }
                else if(iRet > 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;
                    }
                }
                else
                {
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    delete pElm;
                    pElm = NULL;
                }
            }
            else if(event->events & EPOLLOUT)
            {
                iRet = pHandler->HandleSendData(pElm);
                if(iRet == 0)
                {
                    if(bConnectionClose)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        delete pElm;
                        pElm = NULL;

                    }
                    else
                    {
                        if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                        {
                            pEpoller->DelFd(iFd);
                            close(iFd);
                            delete pElm;
                            pElm = NULL;
                        }
                    }
                }
                else if(iRet < 0)
                {
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    delete pElm;
                    pElm = NULL;
                }
            }
            else
            {
                if(!pElm->IsProcessing())
                {
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    delete pElm;
                    pElm = NULL;
                }
            }
        }
    }

    delete pQueue;
    pQueue = NULL;
    delete ioArg;
    ioArg = NULL;
    return NULL;
}

static void* RunAcceptThread(void* arg)
{
    AcptThreadArg *pAcptArg = (AcptThreadArg *)arg;
    auto_ptr<AcptThreadArg> ptrAcpt(pAcptArg);

    TEpoller ** ppEpoller = pAcptArg->ppEpoller;

    int iIOThreadIdx = 0;
    TEpoller *epoller = new TEpoller;
    auto_ptr<TEpoller> ptrEpoller(epoller);
    if(epoller->Create() != 0)
    {
        printf("Create error!\n");
        return NULL;
    }

    vector<TElement *> vecListenElements(pAcptArg->vecListenFd.size());

    for(size_t i = 0; i != pAcptArg->vecListenFd.size(); ++i)
    {
        int iFd = pAcptArg->vecListenFd[i];
        if(TEpollUtils::SetNonBlock(iFd) != 0)
        {
            printf("SetNonBlock error!\n");
            return NULL;
        }
        printf("listen fd :%d\n", iFd);

        //进程结束自动释放
        TElement * pElm = new TElement;
        *(pElm->GetListenFd()) = iFd;

        if(epoller->AddFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR, pElm) != 0)
        {
            delete pElm;
            pElm = NULL;
            printf("listen AddFd error!\n");
            return NULL;
        }
        vecListenElements[i] = pElm;
    }

    struct sockaddr_in sockin;
    socklen_t socklen = sizeof(struct sockaddr_in);
    while(bIsMainRunning)
    {
        int iEvents = epoller->Wait(EPOLL_WAIT_TIMEOUT);
        if(iEvents > 0)
        {
            printf("new connect socket events %d\n", iEvents);
        }
        for(int i = 0; i < iEvents; ++i)
        {
            struct epoll_event * event = epoller->GetEvent(i);
            TElement * pData = (TElement *) event->data.ptr;
            int iListenFd = *(pData->GetListenFd());

            int iFd = accept(iListenFd, (struct sockaddr*)&sockin, &socklen);
            if(iFd == -1)
            {
                //进程创建文件数超过限制后，监听队列里面的新连接不会被取出，所以会一直accept err,不用打log
                if(errno != EMFILE)
                {
                    perror("accept error!");
                }
                continue;
            }

            if(TEpollUtils::SetNonBlock(iFd) != 0)
            {
                printf("SetNonBlock error!\n");
                close(iFd);
                continue;
            }

            //由IO线程释放
            TElement * pEle = new TElement;
            *(pEle->GetListenFd()) = iFd;

            if(ppEpoller[iIOThreadIdx]->AddFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pEle) != 0)
            {
                printf("accept AddFd error!\n");
                delete pEle;
                close(iFd);
                continue;
            }
            iIOThreadIdx = (iIOThreadIdx+1)%IO_THREAD_NUM;
        }
    }

    delete epoller;
    epoller = NULL;

    for(int i = 0; i < vecListenElements.size(); i++)
    {
        delete vecListenElements[i];
    }
    return NULL;
}

TEpollSvr::TEpollSvr()
{
    bIsMainRunning = false;
}

TEpollSvr::~TEpollSvr()
{
}

int TEpollSvr::Run()
{
    if(!bIsMainRunning)
    {
        return -1;
    }

    TEpollUtils::ForkAsDaemon();

    TEpoller ** ppEpoller = new TEpoller*[IO_THREAD_NUM];
    for(int i = 0; i < IO_THREAD_NUM; i++)
    {
        ppEpoller[i] = new TEpoller;
        if(ppEpoller[i]->Create() != 0)
        {
            printf("Create error!\n");
            return -1;
        }
    }

    TEpollHandler * pHandler = new TEpollHandler;

    //launch io thread
    if(bIsNeedWorker)
    {
        vector<TQueue*> vecQueues(IO_THREAD_NUM);
        for(int i = 0; i < IO_THREAD_NUM; i++)
        {
            //io thread release
            IOThreadArg *arg = new IOThreadArg;
            arg->pEpoller = ppEpoller[i];
            if(WORKER_NUM_PER_IO_THREAD == 1)
            {
                arg->pQueue = new TRingQueue;
            }
            else
            {
                arg->pQueue = new TRingMQueue;
            }
            vecQueues[i] = arg->pQueue;
            arg->pHandler = pHandler;

            pthread_t ioThread;
            pthread_create(&ioThread, NULL, RunIOThread, arg);
            pthread_detach(ioThread);
            printf("io thread run\n");
        }

        for(int i = 0; i < IO_THREAD_NUM; i++)
        {
            for(int j = 0; j < WORKER_NUM_PER_IO_THREAD; j++)
            {
                WorkerThreadArg *arg = new WorkerThreadArg;
                arg->pEpoller = ppEpoller[i];
                arg->pQueue = vecQueues[i];
                arg->pHandler = pHandler;

                pthread_t workerThread;
                pthread_create(&workerThread, NULL, RunWorkerThread, arg);
                pthread_detach(workerThread);
                printf("worker thread run\n");
            }
        }
    }
    else
    {
        for(int i = 0; i < IO_THREAD_NUM; i++)
        {
            MainThreadArg *arg = new MainThreadArg;
            arg->pEpoller = ppEpoller[i];
            pthread_t mainThread;
            pthread_create(&mainThread, NULL, RunMainThread, arg);
            pthread_detach(mainThread);
            printf("io and worker thread run\n");
        }
    }

    //launch accept thread
    AcptThreadArg *pAcptArg = new AcptThreadArg;
    pthread_t acptThread;

    int iListenFd = TEpollUtils::Listen(NULL, LISTEN_PORT, 1024);
    if(iListenFd < 0)
    {
        delete pAcptArg;
        pAcptArg = NULL;
        Stop();
        return -1;
    }
    pAcptArg->vecListenFd.push_back(iListenFd);
    pAcptArg->ppEpoller = ppEpoller;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (size_t)2*1024*1024);

    pthread_create( &acptThread, &attr, RunAcceptThread, pAcptArg);

    printf("accept thread run\n");

    pthread_join(acptThread, NULL);

    //sweep

    for(int i = 0; i < IO_THREAD_NUM; i++)
    {
        delete ppEpoller[i];
        ppEpoller[i] = NULL;
    }

    delete pAcptArg;
    pAcptArg = NULL;
    delete[] ppEpoller;
    ppEpoller = NULL;
    delete pHandler;
    pHandler = NULL;
    return 0;
}

int TEpollSvr::Stop()
{
    bIsMainRunning = false;
    return 0;
}

int TEpollSvr::Init()
{
    bIsMainRunning = true;
    return 0;
}

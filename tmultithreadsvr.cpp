#include <netinet/in.h>
#include <memory>
#include "tmultithreadsvr.h"
#include "tepollutils.h"
#include "tepoller.h"
#include "tqueue.h"
#include "tepollhandler.h"
#include "tepollsvrenum.h"
#include "tepollctx.h"
#include "tdatacache.h"

#include <stdio.h>
#include <vector>
#include <errno.h>
#include <syslog.h>
#include <assert.h>


//如果svr接受数据和发送数据达到堆满了缓冲区，此数据包会卡住，
//客户端可以发一个极大的包，然后不read直接close，就能重现
//所以这里需要一个定时清理过期fd的线程

//加上超时处理之后
//超时线程在查超时的包，会先判断时间timeval是否为0，不为0再判断是否超时[24;74H
//这时候其他线程也有可能对这个时间初始化，这样会线程不安全，
//比如说，超时线程A判断timeval不为0， 然后这时候B线程插入处理，
//对timeval处理为0，之后A再去判断是否超时。
//这时候就会超时，如果这时候被线程B清0后，此包又被其他请求使用，
//之后对此包的判断都正常，则会误删其他的请求的数据。


const bool bConnectionClose = false;
const bool bIsNeedWorker = false;


using namespace std;

struct AcptThreadArg
{
    vector<int> vecListenFd;
    TEpollCtx ** ppEpollCtx;
    TDataCache * pDataCache;
};

struct MainThreadArg
{
    TEpollCtx * pEpollCtx;
    TDataCache * pDataCache;
};

struct IOThreadArg
{
    TEpollCtx * pEpollCtx;
    TQueue * pQueue;
    TDataCache * pDataCache;
};

struct WorkerThreadArg
{
    TEpollCtx * pEpollCtx;
    TQueue *pQueue;
};


struct TimeOutThreadArg
{
    TEpollCtx ** ppEpollCtx;
    TDataCache * pDataCache;
};

static bool bIsMainRunning =false;

static void* RunWorkerThread(void* arg);
static void* RunMainThread(void* arg);
static void* RunIOThread(void* arg);
static void* RunAcceptThread(void* arg);
static void* RunTimeOutThread(void* arg);


static void* RunTimeOutThread(void* arg)
{
    struct TimeOutThreadArg * pTimeOutArg = (struct TimeOutThreadArg *)arg;
    TEpollCtx ** ppEpollCtx = pTimeOutArg->ppEpollCtx;
    TDataCache * pDataCache = pTimeOutArg->pDataCache;

    TElement * pElm = NULL;
    TEpollCtx * pEpollCtx = NULL;
    while(bIsMainRunning)
    {
        pElm = pDataCache->GetTimeOutElm(TIME_OUT * 1000);
        if(pElm)
        {
            //may core here idx < 0
            if(pElm->GetIOThreadIdx() >= 0)
            {
                pEpollCtx = ppEpollCtx[pElm->GetIOThreadIdx()];
                printf("ctx %x elm %x idx %d fd %d\n", pEpollCtx, pElm, pElm->GetIOThreadIdx(), *(pElm->GetListenFd()));
                if(!pEpollCtx->HasTimeOutElm())
                {
                    pEpollCtx->SetTimeOutElm(pElm);
                }
            }
        }

        if(!bIsMainRunning) break;

        usleep(200000);
    }

    delete pTimeOutArg;
    return NULL;
}

static void* RunWorkerThread(void* arg)
{
    struct WorkerThreadArg * workerArg = (struct WorkerThreadArg *)arg;
    TEpollCtx * pEpollCtx = workerArg->pEpollCtx;
    TQueue * pQueue = workerArg->pQueue;

    int iFd = -1;
    TElement * pElm = NULL;
    while(bIsMainRunning && pQueue->Pop(&pElm) && pElm)
    {
        if(pElm->IsProcessing())
        {
            if(pElm->IsInQueueTimeOut())
            {
                printf("inqueue timeout, ignore the pkg, %s->%d",
                        __func__, __LINE__);
            }
            else
            {
                pEpollCtx->GetHandler()->DoWork(pElm);
            }

            iFd = *(pElm->GetListenFd());
            if(pEpollCtx->GetEpoller()->ModifyFd(iFd, EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
            {
                printf("ModifyFd err! %s->%d, iFd %d", __func__, __LINE__, iFd);
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
    TEpollCtx * pEpollCtx = mainArg->pEpollCtx;
    TEpoller * pEpoller = pEpollCtx->GetEpoller();
    TEpollHandler * pHandler = pEpollCtx->GetHandler();
    TDataCache * pDataCache = mainArg->pDataCache;

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
            pElm->SetNowTime();

            if(event->events & EPOLLIN)
            {
                iRet = pHandler->HandleRecvData(pElm, true);
                if(iRet == 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLOUT | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        printf("ModifyFd err! %s->%d", __func__, __LINE__);
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        pDataCache->Free(pElm);
                    }
                }
                else if(iRet > 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        printf("ModifyFd err! %s->%d", __func__, __LINE__);
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        pDataCache->Free(pElm);
                    }
                }
                else
                {
                    if(iRet != -1002)
                    {
                        printf("HandleRecvData err! %s->%d iRet %d", __func__, __LINE__, iRet);
                    }

                    pEpoller->DelFd(iFd);
                    close(iFd);
                    pDataCache->Free(pElm);
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
                        pDataCache->Free(pElm);
                    }
                    else
                    {
                        pElm->ClearReqPkg();
                        if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                        {
                            printf("ModifyFd err! %s->%d", __func__, __LINE__);
                            pEpoller->DelFd(iFd);
                            close(iFd);
                            pDataCache->Free(pElm);
                        }
                    }
                }
                else if(iRet < 0)
                {
                    printf("HandleSendData err! %s->%d", __func__, __LINE__);
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    pDataCache->Free(pElm);
                }
            }
            else
            {
                printf("ERR event! %s->%d", __func__, __LINE__);
                pEpoller->DelFd(iFd);
                close(iFd);
                pDataCache->Free(pElm);
            }
        }

        if(!bIsMainRunning) break;

        TElement * pData = pEpollCtx->GetTimeOutElm();

        if(pData)
        {
            iFd = *(pData->GetListenFd());

            if(pData->IsUsed() && iFd > 0 && pData->GetIOThreadIdx() == pEpollCtx->GetIOThreadIdx())
            {
                pEpoller->DelFd(iFd);
                close(iFd);
                pDataCache->Free(pData);
            }
            pEpollCtx->DelTimeOutElm();
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
    TEpollCtx * pEpollCtx = ioArg->pEpollCtx;
    TEpoller *pEpoller =pEpollCtx->GetEpoller();
    TQueue * pQueue = ioArg->pQueue;
    TEpollHandler *pHandler = pEpollCtx->GetHandler();
    TDataCache * pDataCache = ioArg->pDataCache;

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
            pElm->SetNowTime();

            if(event->events & EPOLLIN)
            {
                //worker正在处理
                if(pElm->IsProcessing())    continue;
                iRet = pHandler->HandleRecvData(pElm);
                if(iRet == 0)
                {
                    pElm->SetProcessing();
                    pElm->SetInQueueTime();
                    if(!pQueue->Push(pElm))
                    {
                        //队列满了就删掉有数据的fd，保护一下
                        printf("Push err! %s->%d", __func__, __LINE__);
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        pDataCache->Free(pElm);
                    }
                }
                else if(iRet > 0)
                {
                    if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                    {
                        printf("ModifyFd err! %s->%d", __func__, __LINE__);
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        pDataCache->Free(pElm);
                    }
                }
                else
                {
                    if(iRet != -1002)
                    {
                        printf("HandleRecvData err! %s->%d, iRet %d", __func__, __LINE__, iRet);
                    }

                    pEpoller->DelFd(iFd);
                    close(iFd);
                    pDataCache->Free(pElm);
                }
            }
            else if(event->events & EPOLLOUT)
            {
                pElm->UnSetProcessing();
                iRet = pHandler->HandleSendData(pElm);
                if(iRet == 0)
                {
                    if(bConnectionClose)
                    {
                        pEpoller->DelFd(iFd);
                        close(iFd);
                        pDataCache->Free(pElm);

                    }
                    else
                    {
                        pElm->ClearReqPkg();
                        if(pEpoller->ModifyFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
                        {
                            printf("ModifyFd err! %s->%d", __func__, __LINE__);
                            pEpoller->DelFd(iFd);
                            close(iFd);
                            pDataCache->Free(pElm);
                        }
                    }
                }
                else if(iRet < 0)
                {
                    printf("HandleSendData err! %s->%d iRet %d", __func__, __LINE__, iRet);
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    pDataCache->Free(pElm);
                }
            }
            else
            {
                //worker可能正在处理
                if(!pElm->IsProcessing())
                {
                    printf("ERR event! %s->%d", __func__, __LINE__);
                    pEpoller->DelFd(iFd);
                    close(iFd);
                    pDataCache->Free(pElm);
                }
            }
        }

        TElement * pData = pEpollCtx->GetTimeOutElm();

        if(!bIsMainRunning) break;

        if(pData)
        {
            iFd = *(pData->GetListenFd());

            if(pData->IsUsed() && iFd > 0 && pData->GetIOThreadIdx() == pEpollCtx->GetIOThreadIdx())
            {
                pEpoller->DelFd(iFd);
                close(iFd);
                pDataCache->Free(pData);
            }
            pEpollCtx->DelTimeOutElm();
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

    TEpollCtx ** ppEpollCtx = pAcptArg->ppEpollCtx;
    TDataCache * pDataCache = pAcptArg->pDataCache;

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

        TElement * pElm = pDataCache->Alloc();
        if(pElm == NULL)
        {
            printf("data cache alloc err!\n");
            return NULL;
        }
        *(pElm->GetListenFd()) = iFd;
        pElm->ClearTime();  // no timeout

        if(epoller->AddFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR, pElm) != 0)
        {
            pDataCache->Free(pElm);
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
            TElement * pElm = pDataCache->Alloc();
            if(pElm == NULL)
            {
                printf("data cache alloc err!\n");
                close(iFd);
                continue;
            }

            *(pElm->GetListenFd()) = iFd;
            pElm->SetNowTime();
            pElm->SetIOThreadIdx(iIOThreadIdx);

            if(ppEpollCtx[iIOThreadIdx]->GetEpoller()->AddFd(iFd, EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET, pElm) != 0)
            {
                printf("accept AddFd error!\n");
                pDataCache->Free(pElm);
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
    openlog("testsvr", LOG_PID, LOG_USER);
    bIsMainRunning = false;
}

TEpollSvr::~TEpollSvr()
{
    closelog();
}

int TEpollSvr::Run()
{
    if(!bIsMainRunning)
    {
        return -1;
    }

    TEpollUtils::ForkAsDaemon();

    TDataCache * pDataCache = new TDataCache;
    assert(pDataCache != NULL);

    TEpollCtx ** ppEpollCtx = new TEpollCtx*[IO_THREAD_NUM];
    assert(ppEpollCtx != NULL);

    for(int i = 0; i < IO_THREAD_NUM; i++)
    {
        ppEpollCtx[i] = new TEpollCtx;
        assert(ppEpollCtx[i] != NULL);
        ppEpollCtx[i]->SetIOThreadIdx(i);
    }

    //launch io thread
    if(bIsNeedWorker)
    {
        vector<TQueue*> vecQueues(IO_THREAD_NUM);
        for(int i = 0; i < IO_THREAD_NUM; i++)
        {
            //io thread release
            IOThreadArg *arg = new IOThreadArg;
            arg->pEpollCtx = ppEpollCtx[i];
            arg->pDataCache = pDataCache;
            if(WORKER_NUM_PER_IO_THREAD == 1)
            {
                arg->pQueue = new TRingQueue;
            }
            else
            {
                arg->pQueue = new TRingMQueue;
            }
            vecQueues[i] = arg->pQueue;

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
                arg->pEpollCtx = ppEpollCtx[i];
                arg->pQueue = vecQueues[i];

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
            arg->pEpollCtx = ppEpollCtx[i];
            arg->pDataCache = pDataCache;
            pthread_t mainThread;
            pthread_create(&mainThread, NULL, RunMainThread, arg);
            pthread_detach(mainThread);
            printf("io and worker thread run\n");
        }
    }

    //launch check timeout thread

    struct TimeOutThreadArg * pTimeOutArg = new TimeOutThreadArg;
    pTimeOutArg->ppEpollCtx = ppEpollCtx;
    pTimeOutArg->pDataCache = pDataCache;

    pthread_t timeoutThread;
    pthread_attr_t timeout_attr;
    pthread_attr_init(&timeout_attr);
    pthread_attr_setstacksize(&timeout_attr, (size_t)2*1024*1024);
    pthread_create( &timeoutThread, &timeout_attr, RunTimeOutThread, (void*)pTimeOutArg);
    pthread_detach(timeoutThread);


    //launch accept thread
    AcptThreadArg *pAcptArg = new AcptThreadArg;
    pthread_t acptThread;

    int iListenFd = TEpollUtils::Listen(NULL, LISTEN_PORT, 1024);
    if(iListenFd < 0)
    {
        printf("listen err, errno %d %s\n", errno, strerror(errno));
        delete pAcptArg;
        pAcptArg = NULL;
        Stop();
        return -1;
    }
    pAcptArg->vecListenFd.push_back(iListenFd);
    pAcptArg->ppEpollCtx = ppEpollCtx;
    pAcptArg->pDataCache = pDataCache;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (size_t)2*1024*1024);

    pthread_create( &acptThread, &attr, RunAcceptThread, pAcptArg);

    printf("accept thread run\n");

    pthread_join(acptThread, NULL);

    //sweep

    for(int i = 0; i < IO_THREAD_NUM; i++)
    {
        delete ppEpollCtx[i];
        ppEpollCtx[i] = NULL;
    }

    delete pAcptArg;
    pAcptArg = NULL;

    delete[] ppEpollCtx;
    ppEpollCtx = NULL;

    delete pDataCache;
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
    TEpollUtils::InitSignalHandler();
    return 0;
}



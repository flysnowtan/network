#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>

#include "tepoller.h"
TEpoller::TEpoller()
    :m_EpollFd(-1)
{
}

TEpoller::~TEpoller()
{
    if(m_EpollFd >= 0)
    {   
        close(m_EpollFd);
        m_EpollFd = -1; 
    }   
}

int TEpoller::Create()
{
    m_EpollFd = epoll_create(1024);
    if(-1 == m_EpollFd)
    {   
        perror("epoll_create failed!");
        return -1; 
    }   
    return 0;
}

int TEpoller::AddFd(int iFd, int iFlag, void *pData)
{
    return Control(iFd, EPOLL_CTL_ADD, iFlag, pData);
}

int TEpoller::DelFd(int iFd)
{
    return Control(iFd, EPOLL_CTL_DEL, 0, NULL);
}

int TEpoller::Wait(int iTimeOut)
{
    int iNum = epoll_wait(m_EpollFd, m_Events, MAX_EPOLL_EVENTS, iTimeOut);
    if(iNum >= 0) return iNum;
    if(errno == EINTR)  return 0;
    perror("epoll_wait failed!");
    return -1; 
}

int TEpoller::ModifyFd(int iFd, int iFlag, void *pData)
{
    return Control(iFd, EPOLL_CTL_MOD, iFlag, pData);
}

struct epoll_event * TEpoller::GetEvent(int iEventIdx)
{
    assert(iEventIdx < MAX_EPOLL_EVENTS);
    return &(m_Events[iEventIdx]);
}

int TEpoller::Control(int iFd, int iOp, int iFlag, void *pData)
{
    if(m_EpollFd <= 0 || iFd < 0)
    {   
        printf("Control error! m_EpollFd %d iFd %d\n", m_EpollFd, iFd);
        return -1; 
    }   

    struct epoll_event event;
    event.events = iFlag;
    event.data.ptr = pData;

    return epoll_ctl(m_EpollFd, iOp, iFd, &event);
}


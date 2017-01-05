#ifndef __T__EPOLLER__H__
#define __T__EPOLLER__H__

#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>


//为了简单，都没有打日志
#define MAX_EPOLL_EVENTS 20480
class TEpoller
{
public:
    TEpoller() 
        :m_EpollFd(-1)
    {   
    
    }   

    ~TEpoller()
    {   
        if(m_EpollFd >= 0)
        {   
            close(m_EpollFd);
            m_EpollFd = -1; 
        }   
    }   
    
    int Create()
    {   
        m_EpollFd = epoll_create(1024);
        if(-1 == m_EpollFd)
        {   
            return -1; 
        }   
        return 0;
    }   

    int AddFd(int iFd, int iFlag, void *pData)
    {   
        return Control(iFd, EPOLL_CTL_ADD, iFlag, pData);
    }   

    int DelFd(int iFd)
    {   
        return Control(iFd, EPOLL_CTL_DEL, 0, NULL);
    }   

    int Wait(int iTimeOut)
    {   
        int ret = epoll_wait(m_EpollFd, m_Events, MAX_EPOLL_EVENTS, iTimeOut);
        if(ret >= 0)    return ret;
        if(ret == -1 && errno != EINTR) return -1; 
        return 0;
    }   

    int ModifyFd(int iFd, int iFlag, void *pData)
    {   
        return Control(iFd, EPOLL_CTL_MOD, iFlag, pData);
    }   

    struct epoll_event * GetEvent(int iIdx)
    {   
        assert(iIdx < MAX_EPOLL_EVENTS);
        return &(m_Events[iIdx]);
    }   

private:
    int Control(int iFd, int iOp, int iFlag, void *pData)
    {   
        if(m_EpollFd < 0 || iFd < 0)
        {
            return -1;
        }

        struct epoll_event event;
        event.events = iFlag;
        event.data.ptr = pData; //把fd和用户数据都放在pData中
        return epoll_ctl(m_EpollFd, iOp, iFd, &event);
    }

    int m_EpollFd;
    struct epoll_event m_Events[MAX_EPOLL_EVENTS];
};

#endif

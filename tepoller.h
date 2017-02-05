#pragma once

#include <sys/epoll.h>
#include "tepollsvrenum.h"

//为了简单，都没有打日志
class TEpoller
{
    public:
        TEpoller();
        ~TEpoller();
        int Create();
        int AddFd(int iFd, int iFlag, void *pData);
        int DelFd(int iFd);
        int Wait(int iTimeOut);
        int ModifyFd(int iFd, int iFlag, void *pData);
        struct epoll_event * GetEvent(int iEventIdx);
    private:
        int Control(int iFd, int iOp, int iFlag, void *pData);

    private:
        int m_EpollFd;
        struct epoll_event m_Events[MAX_EPOLL_EVENTS];
};

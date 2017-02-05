#pragma once

#include "tepoller.h"
#include "tepollhandler.h"
#include "telement.h"

class TEpollCtx
{
    public:
        TEpollCtx();
        ~TEpollCtx();

        void SetIOThreadIdx(int idx);
        int GetIOThreadIdx();

        TEpoller * GetEpoller();
        TEpollHandler * GetHandler();
        TElement * GetTimeOutElm();

        bool HasTimeOutElm();
        void SetTimeOutElm(TElement * pElm);
        void DelTimeOutElm();

    private:

        int m_IOThreadIdx;
        TEpoller * m_pEpoller;
        TEpollHandler * m_pHandler;
        TElement * m_pTimeOutElm;
};

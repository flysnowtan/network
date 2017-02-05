#pragma once

#include "telement.h"
#include "tqueue.h"

class TDataCache
{
    public:
        TDataCache();
        ~TDataCache();
        TElement * Alloc();
        bool Free(TElement * pElm, bool bWait = true);

        TElement * GetTimeOutElm(uint32_t msec);
    private:

    private:
        TElement ** m_ppBlocks;
        TElement * m_pFreeList;

        uint32_t m_AllocPos;
        uint32_t m_CheckPos;

        //这里最好使用CAS实现的无锁队列
        TRingMQueue m_FreeQueue;
};

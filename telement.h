#pragma once

#include "tbuffer.h"
#include "tepollsvrenum.h"
#include<sys/time.h>


class TElement
{
    public:
        enum ELEMENT_FLAG
        {   
            NOTHING = 0,
            IS_PROCESSING = 1 << 0,
            IS_USED = 1 << 1,
        };  

    public:
        TElement();
        TElement(TBufferPool * pPool);
        ~TElement();

    public:
        int *GetListenFd();

        TBuffer* GetReqPkg();
        TBuffer* GetRspPkg();

        void SetProcessing();
        bool IsProcessing();
        void UnSetProcessing();
        bool IsUsed();
        void SetUsed();

        struct timeval * GetInQueueTime();
        struct timeval * GetTime();
        bool IsInQueueTimeOut();
        bool IsReadTimeOut();
        bool IsWriteTimeOut();
        bool IsTimeOut(uint32_t msec);
        void SetInQueueTime();
        void SetNowTime();

        void Clear();
        void ClearFlag();
        void ClearReqPkg();
        void ClearRspPkg();
        void ClearTime();

        void Reset();

        void SetIOThreadIdx(int idx);
        int GetIOThreadIdx();

    private:
        int m_IOThreadIdx;
        int m_ListenFd;
        int m_Flag;
        TBuffer m_ReqPkg;
        TBuffer m_RspPkg;
        struct timeval m_InQueueTime;
        struct timeval m_Time;
    public:
        TElement * m_pNext;
};

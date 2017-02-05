#pragma once

#include <stdint.h>
#include <unistd.h>

#include "tmutexlock.h"
#include "telement.h"
#include <iostream>

class TQueue
{
    typedef TElement * T;
    public:
        TQueue() {}
        virtual ~TQueue() {}
        virtual bool Push(const T &data) = 0;
        virtual bool Pop(T *data, bool bWait = true) = 0;
};

//单进单出队列
class TRingQueue : public TQueue
{
    typedef TElement * T;
    public:
    //队列长度为1<<iBitSize
    TRingQueue(const int iBitSize = 20) 
        :m_WriteIdx(0), m_ReadIdx(0)
    {   
        m_DataSize = 1 << iBitSize;
        m_pData = new T[1 << iBitSize];

        for(uint64_t i = 0; i != m_DataSize; ++i)
        {   
            m_pData[i] = NULL;
        }   
    }   

    ~TRingQueue()
    {   
        delete[] m_pData;
    }   

    uint64_t Size() 
    {   
        return m_WriteIdx - m_ReadIdx;
    }   

    bool Push(const T &data)
    {   
        if(m_WriteIdx - m_ReadIdx < m_DataSize)
        {   
            m_pData[m_WriteIdx & (m_DataSize - 1)] = data;
            __sync_synchronize();
            ++m_WriteIdx;
            return true;
        }   
        return false;
    }   
    
    bool Pop(T *data, bool bWait = true)
    {   
        while(true)
        {   
            if(m_ReadIdx < m_WriteIdx)
            {   
                *data = m_pData[m_ReadIdx & (m_DataSize - 1)];
                __sync_synchronize();
                ++m_ReadIdx;
                return true;
            }   

            if(!bWait) return false;
            usleep(10000);
        }
        return false;
    }

    private:
    volatile uint64_t m_WriteIdx;
    volatile uint64_t m_ReadIdx;
    T* m_pData;
    uint64_t m_DataSize;
};

class TRingMQueue : public TQueue
{
    typedef TElement * T;
    public:
    //队列长度为1<<iBitSize
    TRingMQueue(const int iBitSize = 20)
        :m_WriteIdx(0), m_ReadIdx(0)
    {
        m_DataSize = 1 << iBitSize;
        m_pData = new T[1 << iBitSize];

        for(uint64_t i = 0; i != m_DataSize; ++i)
        {
            m_pData[i] = NULL;
        }
    }

    ~TRingMQueue()
    {
        delete[] m_pData;
    }

    uint64_t Size()
    {
        return m_WriteIdx - m_ReadIdx;
    }

    bool Push(const T &data)
    {
        TMutexLockHelper lockHelper(m_PushLock);
        if(m_WriteIdx - m_ReadIdx < m_DataSize)
        {
            m_pData[m_WriteIdx & (m_DataSize - 1)] = data;
            ++m_WriteIdx;
            return true;
        }
        return false;
    }
    
    bool Pop(T *data, bool bWait = true)
    {
        while(true)
        {
            {
                TMutexLockHelper lockHelper(m_PopLock);
                if(m_ReadIdx < m_WriteIdx)
                {
                    *data = m_pData[m_ReadIdx & (m_DataSize - 1)];
                    ++m_ReadIdx;
                    return true;
                }
            }

            if(!bWait) return false;
            usleep(10000);
        }
        return false;
    }

    private:
    uint64_t m_WriteIdx;
    uint64_t m_ReadIdx;
    T* m_pData;
    uint64_t m_DataSize;
    TMutexLock m_PushLock;
    TMutexLock m_PopLock;
};


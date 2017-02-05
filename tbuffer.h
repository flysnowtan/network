#pragma once

#include <stdint.h>
#include <string>

class TBufferPool
{
    public:
        TBufferPool() {}
        ~TBufferPool() {}
        virtual char * Alloc(const uint32_t iSize, uint32_t * pRealSize = NULL);
        virtual void Dealloc(char * pMem);
};

class TBuffer
{
    public:
        TBuffer();
        TBuffer(TBufferPool * pPool);
        ~TBuffer();
        void ExtendSize(uint32_t iSize);
        uint32_t Write(const char * pBuffer, uint32_t iSize);
        uint32_t Write(const std::string & str);

        char * GetWritePos(uint32_t iSize);
        char * GetWritePos();
        char * GetReadPos();

        void AddReadPos(uint32_t iSize);
        void AddWritePos(uint32_t iSize);

        void SetReadPos(uint32_t iSize);
        void SetWritePos(uint32_t iSize);

        uint32_t GetUnReadSize();
        uint32_t GetCapacity();
        uint32_t GetLen();
        char * GetBuffer();

        void Reset();

    private:
        TBuffer(const TBuffer &other);
        TBuffer & operator = (TBuffer &other);

    private:
        TBufferPool * m_pPool;
        char * m_pMem;
        uint32_t m_iReadPos;
        uint32_t m_iWritePos;
        uint32_t m_iCapacity;
        bool m_bDefaultPool;
};

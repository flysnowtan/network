#pragma once

#include "telement.h"

class TEpollHandler
{
    public:
        int HandleRecvData(TElement *pElm, bool bDoWork = false);
        int HandleSendData(TElement *pElm);
        int DoWork(TElement *pElm);

        int IsCompletedPkg(TElement *pElm);
    private:
};

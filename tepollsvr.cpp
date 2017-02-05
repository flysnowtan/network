#include "tmultithreadsvr.h"
#include <stdio.h>

int main(void)
{
    setbuf(stdout, NULL);
    TEpollSvr svr;
    int iRet = svr.Init();
    if(iRet != 0)
    {   
        printf("ERR! init err\n");
        return 0;
    }   

    svr.Run();

    return 0;
}

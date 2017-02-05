objects = tqueue.o tmultithreadsvr.o telement.o tmutexlock.o tepollutils.o tepoller.o 

CC = g++ 

all: epollsvr

testsvr: testsvr.cpp tepollutils.cpp tepoller.cpp  tmultithreadsvr.cpp tepollhandler.cpp  tbuffer.cpp tepollctx.cpp
    $(CC) -o testsvr testsvr.cpp  tepollutils.cpp tepoller.cpp tmultithreadsvr.cpp tepollhandler.cpp  tbuffer.cpp tepollctx.cpp -pthread -g -O0

epollsvr: tepollsvr.cpp tepollutils.cpp tepoller.cpp  tmultithreadsvr.cpp tepollhandler.cpp  tbuffer.cpp telement.cpp tepollctx.cpp tdatacache.cpp
    $(CC) -o epollsvr tepollsvr.cpp  tepollutils.cpp tepoller.cpp tmultithreadsvr.cpp tepollhandler.cpp  tbuffer.cpp telement.cpp tepollctx.cpp tdatacache.cpp -pthread -g -O0

queue: queue.cpp tqueue.h tmutexlock.h
    $(CC)  queue.cpp -lpthread -o queue

tmultithreadsvr.o : tepoller.h telement.h tepollutils.h

tepoller.o : tepoller.h
    $(CC) -c tepoller.h -o tepoller.o

tqueue.o : tqueue.h tmutexlock.h
    $(CC) -c tqueue.h -o tqueue.o

telement.o : telement.cpp telement.h 
    $(CC) -c telement.cpp -o telement.o 

tmutexlock.o : tmutexlock.h
    $(CC) -c tmutexlock.h -o tmutexlock.o

tepollutils.o : tepollutils.h

tbuffer.o : tbuffer.cpp tbuffer.h
    $(CC) -c tbuffer.cpp -o tbuffer.o

tepollctx.o : tepollctx.cpp tepollctx.h
    $(CC) -c tepollctx.cpp -o tepollctx.o

tdatacache.o : tdatacache.cpp tdatacache.h
    $(CC) -c tdatacache.cpp -o tdatacache.o

clean:  
    rm -f $(objects)

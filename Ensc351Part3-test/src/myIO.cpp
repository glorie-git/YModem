//============================================================================
//
//% Student Name 1: Glorie Ramazani
//% Student 1 #: 301371913
//% Student 1 userid (email): gra16 (gra16@sfu.ca)
//
//% Student Name 2: Uchechi Nwasike
//% Student 2 #: 301297181
//% Student 2 userid (email): unwasike (unwasike@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P3_<userid1>_<userid2>" (eg. P3_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cpp
// Version     : September 28, 2021
// Description : Wrapper I/O functions for ENSC-351
// Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <unistd.h>         // for read/write/close
#include <fcntl.h>          // for open/creat
#include <sys/socket.h>     // for socketpair
#include <stdarg.h>         // for va stuff
#include <iostream>
#include <vector>
#include "VNPE.h"
#include <stdlib.h>             // for exit()
#include <sys/socket.h>
#include <mutex>
#include <condition_variable>
#include "RageUtil_CircularBuffer.h"
#include <termios.h>

#include "SocketReadcond.h"

using namespace std;

namespace {

class socketDrainClass;

typedef vector<socketDrainClass*> sdcVectType;
sdcVectType desInfoVect(3);

mutex vectMutex;

class socketDrainClass {
    int buffered = 0;
    CircBuf<char> buffer;
    condition_variable cvDrain;
    condition_variable cvRead;
    mutex socketDrainMutex;

public:
    int pair;   // cannot be private because of myTcdrain and myWrite

    socketDrainClass(unsigned pairInit)
    :buffered(0),pair(pairInit){
        buffer.reserve(300);
    }

    // make the calling thread to wait for a reading thread to drain the data
    int waitingForDraining(unique_lock<mutex>& passedVectlk){

        unique_lock<mutex> condlk(socketDrainMutex);
        passedVectlk.unlock();
        if (buffered > 0){
            cvDrain.wait(condlk, [this] {return buffered <= 0;});
        }

        return 0;
    }

    // writing on a descriptor and update the reading buffered
    int writing(int des, const void* buf, size_t nbyte){
        lock_guard<mutex> condlk(socketDrainMutex);
        int written  = buffer.write((const char*)buf, nbyte);
        buffered += written;
        if (buffered >= 0) {
            cvRead.notify_one();
        }
        return written;
    }

    // checking buffered and waking up the wait-for-draining threads if needed
    int reading(int des, void *buf, int n, int min, int time, int timeout){

        int bytesRead;
        unique_lock<mutex> condlk(socketDrainMutex);
        if (buffered >= min || pair == -1) {

            bytesRead = buffer.read((char *) buf, n);

            if (bytesRead > 0 ){
                buffered -= bytesRead;
                if(!buffered){
                    cvDrain.notify_all();
                }
            }
        } else {
            if (buffered < 0 ) {
//                COUT << "Currently only support one reading call at a time" << endl;
                exit(EXIT_FAILURE);
            }
            if (time != 0 || timeout != 0) {
//                COUT << "Currently only supporting no timeouts or immediate timeouts" << endl;
                exit(EXIT_FAILURE);
            }

            buffered -= min;
            cvDrain.notify_all(); // buffered must be <= 0

            cvRead.wait(condlk, [this] {return (buffered) >= 0 || pair == -1;});
            bytesRead = buffer.read((char *) buf, n);
            buffered -= (bytesRead - min);

        }
        return bytesRead;
    }

    int finishClosing(int des, socketDrainClass* des_pair){
        int retVal = close(des);

        if(retVal != -1){
            if(des_pair){
                des_pair->pair = -1;
                if (des_pair->buffered < 0){
                    // no more data will be written from des
                    // notify thread waiting on reading on paired descriptor
                    des_pair->cvRead.notify_one();
                } else if(des_pair->buffered > 0){
                    // there shouldn't be any threads waiting in myTcDrain on des
                    // but just incase
                    des_pair->cvDrain.notify_all();
                }
                if (buffered > 0){
                    // by closing the socket we are throwing away any buffered data
                    // notifications are sent immediately to myTcDrain waiters on paired descriptor
                    buffered = 0;
                    cvDrain.notify_all(); // needed? will notifications be sent anyhow to cv destruction?

                } else if(buffered < 0){
                    // there should not be any threads waiting in myReadCon() on des, but just incase

                    buffered = 0;
                    cvDrain.notify_all(); // when will the actual notificatins take place? Too late?

                }
                delete desInfoVect[des];
                desInfoVect[des] = nullptr;
            }
        }
        return retVal;
    }

    // should be done only after all other operations on des have returned
    int closing(int des) {
        // vectMutex already lock at this point
        socketDrainClass * des_pair = nullptr;
        unique_lock<mutex> condlk(socketDrainMutex, defer_lock);
        if (pair == -1){ // it is safe to reference pair here and below because vectMutex is locked
            // the paired descriptor has already been closed, not need to lock mutex
            condlk.lock();
            return finishClosing(des, des_pair);
        } else {
            des_pair = desInfoVect[pair];
            unique_lock<mutex> condPairlk(des_pair->socketDrainMutex, defer_lock);
            lock(condPairlk, condlk);
            return finishClosing(des, des_pair);
        }
    }
};

}


// open a file and get its descriptor. If needed expand the vector
// to fit the new descriptor
// return value of open

int myOpen(const char *pathname, int flags, ...) //, mode_t mode)
{
    lock_guard<mutex> vectlk(vectMutex);
    mode_t mode = 0;
    // in theory we should check here whether mode is needed.
    va_list arg;
    va_start (arg, flags);
    mode = va_arg (arg, mode_t);
    va_end (arg);

    int des = open(pathname, flags, mode);
    if((sdcVectType::size_type) des >= desInfoVect.size()){
            desInfoVect.resize(des+1);
    }
    return des;
}

int myCreat(const char *pathname, mode_t mode)
{
    lock_guard<mutex> vectlk(vectMutex);
    int des = creat(pathname, mode);
    if((sdcVectType::size_type) des >= desInfoVect.size()){
        desInfoVect.resize(des+1);
    }
    return des;
}

// create a pair of sockets and put them in desInfoVect
// returns an integer that indicates if operation was successful (0) or not (-1)
int mySocketpair( int domain, int type, int protocol, int des_array[2] )
{
    lock_guard<mutex> vectlk(vectMutex);
    int returnVal = socketpair(domain, type, protocol, des_array);
    if (returnVal != -1){
        int vectSize = desInfoVect.size();
        int maxDes = max(des_array[0], des_array[1]);
        if (maxDes >= vectSize){
            int vectSize = desInfoVect.size();

            int maxDes = max(des_array[1], des_array[0]);

            if(maxDes >= vectSize){
                desInfoVect.resize(maxDes+1);
            }

            desInfoVect[des_array[0]] = new socketDrainClass(des_array[1]);
            desInfoVect[des_array[1]] = new socketDrainClass(des_array[0]);
        }
    }
    return returnVal;
}

int myReadCond(int des, void* buf, int n, int min, int time, int timeout){
    if(desInfoVect.at(des)){
        return wcsReadcond(des, buf, n, min, time, timeout );
    }
    return desInfoVect.at(des)->reading(des, buf, n, min, time, timeout );
}

// returns number of bytes read or -1 on failure
ssize_t myRead( int des, void* buf, size_t nbyte )
{
    // Check if des obtained from mySocketpair or not
    if(desInfoVect.at(des)){
        return read(des, buf, nbyte );
    }
    return read(des, buf, nbyte );
}

ssize_t myWrite( int des, const void* buf, size_t nbyte )
{
    unique_lock<mutex> vectlk(vectMutex);
    // check if from socket pair or not
    socketDrainClass* desInfoP = desInfoVect.at(des);
    if(desInfoP){
        if(desInfoP->pair == -1){
            errno = EPIPE;
            return -1;
        } else {
            // locking mutex above makes sure that desInfoP is not closed here
            return desInfoVect[desInfoP->pair]->writing(des, buf, nbyte);
        }
    } else {
        return write(des, buf, nbyte );
    }
}

// should not be called until all other calls using descriptor have been called
int myClose( int des )
{
    // prevent clos being called at the same time as socketpair
    // socket pair should not be closed at the same time
    lock_guard<mutex> vectlk(vectMutex);
    if(!desInfoVect.at(des)){
        return close(des);
    }
    return desInfoVect[des]->closing(des);
}


// making the calling thread wait for a reading thread to drain all the data
int myTcdrain(int des)
{ //is also included for purposes of the course.
    unique_lock<mutex> vectlk(vectMutex);
    socketDrainClass* desInfoP = desInfoVect.at(des);
    if(desInfoP && desInfoP->pair != -1){
        return desInfoVect[desInfoP->pair]->waitingForDraining(vectlk);
    }
    vectlk.unlock();

    return tcdrain(des); // des is not from a pair of sockets
}

/* Arguments:
des
    The file descriptor associated with the terminal device that you want to read from.
buf
    A pointer to a buffer into which readcond() can put the data.
n
    The maximum number of bytes to read.
min, time, timeout
    When used in RAW mode, these arguments override the behavior of the MIN and TIME members of the terminal's termios structure. For more information, see...
 *
 *  https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 *  */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
    if(desInfoVect.at(des)){
        return wcsReadcond(des, buf, n, min, time, timeout );
    }
    return desInfoVect.at(des)->reading(des, buf, n, min, time, timeout );
//  return wcsReadcond(des, buf, n, min, time, timeout );
}




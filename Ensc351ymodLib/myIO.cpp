//============================================================================
// Name        : myIO.cpp
// Author(s)   : Craig Scratchley
//			   :
// Version     : November 24, 2021 -- tcdrain for socketpairs.
// Copyright   : Copyright 2021, W. Craig Scratchley, SFU
// Description : An implementation of tcdrain-like behaviour for socketpairs.
//============================================================================

// dangerous in the general case, but uncomment the below to use
//     wcsReadcond to block instead of cvRead condition variable
#define WCSREADCOND
#ifndef WCSREADCOND
// if not using WCSREADCOND, then CIRCBUF is an option, to use
//     a circular buffer instead of read() and write() functions.
//#define CIRCBUF
#endif

// Uncomment the line below to turn on debugging output
//#define REPORT_INFO

#include <sys/socket.h>
#include <unistd.h>				// for posix i/o functions
#include <stdlib.h>
#include <termios.h>			// for tcdrain()
#include <fcntl.h>				// for open/creat
#include <errno.h>
#include <stdarg.h>
#include <mutex>				
#include <shared_mutex>
#include <condition_variable>	
#include <map>
#include <memory>
#include "AtomicCOUT.h"
#include "SocketReadcond.h"
#include "VNPE.h"
#ifdef CIRCBUF
    #include "RageUtil_CircularBuffer.h"
#endif

using namespace std;

//Unnamed namespace
namespace{

    class socketInfoClass;

    typedef shared_ptr<socketInfoClass> socketInfoClassSp;
    map<int, socketInfoClassSp> desInfoMap {
        {-2, make_shared<socketInfoClass>(-1)}, // marker for a descriptor whose pair is closed
        {0, nullptr}, // init for stdin,
        {1, nullptr}, //          stdout,
        {2, nullptr}  //          stderr
    };

    //  A shared mutex used to protect desInfoMap so only a single thread can modify the map at a time.
    //  This also means that only one call to functions like mySocketpair() or myClose() can make progress at a time.
    //  This mutex is also used to prevent a paired socket from being closed at the beginning of a myWrite or myTcdrain function.
    shared_mutex mapMutex;

    class socketInfoClass {
        unsigned totalWritten = 0;
        unsigned maxTotalCanRead = 0;
        condition_variable cvDrain;
    #ifndef WCSREADCOND
        condition_variable cvRead;
    #endif
    #ifdef CIRCBUF
        CircBuf<char> circBuffer;
        bool connectionReset = false;
    #endif
        mutex socketInfoMutex;
    public:
        int pair;   // Cannot be private because myWrite and myTcdrain using it.
                    // -1 when descriptor closed, -2 when paired descriptor is closed

        socketInfoClass(unsigned pairInit)
        :pair(pairInit) {
    #ifdef CIRCBUF
                circBuffer.reserve(1100); // note constant of 1100
    #endif
        }

	/*
	 * Function:  if necessary, make the calling thread wait for a reading thread to drain the data
	 */
	int draining(shared_lock<shared_mutex> &desInfoLk)
	{ // operating on object for paired descriptor of original des
		unique_lock<mutex> socketLk(socketInfoMutex);
		desInfoLk.unlock();

		//  once the reader decides the drainer should wakeup, it should wakeup
		if (pair >= 0 && totalWritten > maxTotalCanRead)
			cvDrain.wait(socketLk); // what about spurious wakeup?
//        cvDrain.wait(socketLk, [this]{return pair < 0 || totalWritten <= maxTotalCanRead;});

		if (pair == -2) {
			errno = EBADF; // check errno
			return -1;
		}
		return 0;
	}

	int writing(int des, const void* buf, size_t nbyte)	{
		// operating on object for paired descriptor
		lock_guard<mutex> socketLk(socketInfoMutex);
#ifdef CIRCBUF
		int written = circBuffer.write((const char*) buf, nbyte);
#else
		int written = write(des, buf, nbyte);
#endif
        if (written > 0) {
            totalWritten += written;
#ifndef WCSREADCOND
            cvRead.notify_one();
#endif
        }
#ifdef REPORT_INFO
        COUT << " mw:" << des << ";" << totalWritten << flush;
#endif
        return written;
	}

	int reading(int des, void * buf, int n, int min, int time, int timeout)
	{
		int bytesRead;
		unique_lock<mutex> socketLk(socketInfoMutex);

		// would not have got this far if pair == -1
		if ((!maxTotalCanRead && totalWritten >= (unsigned) min) || pair == -2) {
#ifdef CIRCBUF
		    if (pair == -2)
	            if (connectionReset) {
	                errno = ECONNRESET;
	                bytesRead = -1;
	                connectionReset = false;
	            }
	            else
	                bytesRead = 0;
		    else {
		        bytesRead = circBuffer.read((char *) buf, n);
#else
            // wcsReadcond should not wait in this situation.
            // bytesRead = wcsReadcond(des, buf, n, min, time, timeout);
		    if (min == 0 && totalWritten == 0 && pair >= 0)
		        bytesRead = 0;
		    else {
		        bytesRead = read(des, buf, n); // at least min will be waiting or pair == -2
#endif
                if (bytesRead > 0) {
                    totalWritten -= bytesRead;
                    if (totalWritten <= maxTotalCanRead /* && pair >= 0 */)
                        cvDrain.notify_all();
                }
			}
		}
		else {
			maxTotalCanRead +=n;
			cvDrain.notify_all(); // totalWritten must be less than min
#ifdef WCSREADCOND
			socketLk.unlock(); // this might be dangerous if there are multiple readers
			// (a) consider myWrite() is called here, with sufficient (extra) bytes.
			// (b1) myWrite() could be called again here, which means that more bytes
			//      could be read, and totalWritten could be 0
			// tcdrain() might have been called here, and might have to block
		    bytesRead = wcsReadcond(des, buf, n, min, time, timeout);
			// (b2) or myWrite() could be called again here, which would definitely
		    //      mean that totalWritten will end up > 0
			// what if tcdrain() is called here, we don't know if it would have to block or not.
			socketLk.lock();
#else
            if (time != 0 || timeout != 0) {
                COUT << "Currently only supporting no timeouts or immediate timeout" << endl;
                exit(EXIT_FAILURE);
            }

			cvRead.wait(socketLk, [this, min] {
			    // after notify, myTcdrain() might have been called, and block
			    return totalWritten >= (unsigned) min || pair < 0;});
            if (pair == -1) { // shouldn't normally happen
                errno = EBADF; // check errno value
                return -1;
            }
#ifdef CIRCBUF
            bytesRead = circBuffer.read((char *) buf, n);    
            if (connectionReset && bytesRead == 0) {
                errno = ECONNRESET;
                bytesRead = -1;
                connectionReset = false;
            }
#else
            // choice below seems to affect "Connection reset by peer"
            bytesRead = read(des, buf, n);
            //bytesRead = wcsReadcond(des, buf, n, min, time, timeout);
#endif // #ifdef CIRCBUF
#endif // #ifdef WCSREADCOND
            
			if (bytesRead != -1)
				totalWritten -= bytesRead;
            maxTotalCanRead -= n;
#ifndef WCSREADCOND
			if (totalWritten > 0 || pair < 0) // || pair == -2
				cvRead.notify_one(); // can this affect errno?
#endif
		}
#ifdef REPORT_INFO
		COUT << " mr:" << des << ";" << bytesRead << ";" << totalWritten << flush;
#endif
		return bytesRead;
	} // .reading()

	/*
	 * Function:  Closing des. Should be done only after all other operations on des have returned.
	 */
	int closing(int des)
	{
		// mapMutex already locked at this point, so no mySocketpair or other myClose
		if(pair != -2) { // pair has not already been closed
			socketInfoClassSp des_pair(desInfoMap[pair]);
			unique_lock<mutex> socketLk(socketInfoMutex, defer_lock);
			unique_lock<mutex> condPairlk(des_pair->socketInfoMutex, defer_lock);
			lock(condPairlk, socketLk);
			pair = -1; // this is first socket in the pair to be closed
			des_pair->pair = -2; // paired socket will be the second of the two to close.
            if (totalWritten > maxTotalCanRead) {
                // by closing the socket we are throwing away any buffered data.
                // notification will be sent immediately below to any myTcdrain waiters on paired descriptor.
#ifdef CIRCBUF
                des_pair->connectionReset = true;
#endif
                cvDrain.notify_all();
            }
#ifndef WCSREADCOND
            if (maxTotalCanRead > 0) {
                // there shouldn't be any threads waiting in myRead() or myReadcond() on des, but just in case.
                cvRead.notify_all();
            }

			if (des_pair->maxTotalCanRead > 0) {
				// no more data will be written from des
				// notify a thread waiting on reading on paired descriptor
				des_pair->cvRead.notify_one();
			}
#endif
			if (des_pair->totalWritten > des_pair->maxTotalCanRead) {
				// there shouldn't be any threads waiting in myTcdrain on des, but just in case.
				des_pair->cvDrain.notify_all();
			}
		}
		// unlock condPairlk here or even above a little bit.
		return close (des);
	} // .closing()
	}; // socketInfoClass

    // get shared pointer for des info
    socketInfoClassSp getDesInfoP(int des) {
        auto iter = desInfoMap.find(des);
        if (iter == desInfoMap.end())
            return nullptr; // des not in use
        else
            return iter->second; // return the shared pointer
    }

    // get shared pointer for des info (locked version)
    socketInfoClassSp lockedGetDesInfoP(int des) {
        shared_lock<shared_mutex> desInfoLk(mapMutex);
        return getDesInfoP(des);
    }

    // get shared pointer for paired des info (locked version)
    socketInfoClassSp getDesPairInfoP(int des) {
        socketInfoClassSp desInfoP = getDesInfoP(des);
        if (!desInfoP)
            return nullptr; // des not in use
        else // since *desInfoP is good, pair != -1
             // locking mapMutex above makes sure that desinfoP->pair is not closed here
            return desInfoMap[desInfoP->pair];
    } // getDesPairInfoP()
} // unnamed namespace

/*
 * Function:	Calling the reading member function to read
 * Return:		An integer with number of bytes read, or -1 for an error.
 * see https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref/topic/r/readcond.html
 *
 */
int myReadcond(int des, void * buf, int n, int min, int time, int timeout) {
	socketInfoClassSp desInfoP = lockedGetDesInfoP(des);
	if (desInfoP)
	    return desInfoP->reading(des, buf, n, min, time, timeout);
    return wcsReadcond(des, buf, n, min, time, timeout);
}

/*
 * Function:	Reading directly from a file or from a socketpair descriptor)
 * Return:		the number of bytes read , or -1 for an error
 */
ssize_t myRead(int des, void* buf, size_t nbyte) {
	socketInfoClassSp desInfoP = lockedGetDesInfoP(des);
	if (desInfoP)
	    // myRead (for sockets) usually reads a minimum of 1 byte
	    return desInfoP->reading(des, buf, nbyte, 1, 0, 0);
    return read(des, buf, nbyte); // des is closed or not from a socketpair

}

/*
 * Return:		the number of bytes written, or -1 for an error
 */
ssize_t myWrite(int des, const void* buf, size_t nbyte) {
    {
        shared_lock<shared_mutex> desInfoLk(mapMutex);
        auto desPairInfoSp = getDesPairInfoP(des); // make a local shared pointer
        if (desPairInfoSp && desPairInfoSp->pair != -1) // switch to -1 to -3?
            return desPairInfoSp->writing(des, buf, nbyte);
    }
    return write(des, buf, nbyte); // des is not from a pair of sockets or socket or pair closed
}

/*
 * Function:  make the calling thread wait for a reading thread to drain the data
 */
int myTcdrain(int des) {
    {
        shared_lock<shared_mutex> desInfoLk(mapMutex);
        auto desPairInfoSp = getDesPairInfoP(des); // make a local shared pointer
        if (desPairInfoSp) {
            if (desPairInfoSp->pair == -1)
                return 0; // paired descriptor is closed.
            else
                return desPairInfoSp->draining(desInfoLk);
        }
    }
    return tcdrain(des); // des is not from a pair of sockets or socket closed
}

/*
 * Function:	Open a file and get its file descriptor.
 * Return:		return value of open
 */
int myOpen(const char *pathname, int flags, ...) //, mode_t mode)
{
    mode_t mode = 0;
    // in theory we should check here whether mode is needed.
    va_list arg;
    va_start (arg, flags);
    mode = va_arg (arg, mode_t);
    va_end (arg);
	int des = open(pathname, flags, mode);
	if (des != -1) {
	    lock_guard<shared_mutex> desInfoLk(mapMutex);
		desInfoMap[des] = nullptr; }
	return des;
}

/*
 * Function:	Create a new file and get its file descriptor.
 * Return:		return value of creat
 */
int myCreat(const char *pathname, mode_t mode)
{
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	int des = creat(pathname, mode);
	if (des != -1)
		desInfoMap[des] = nullptr;
	return des;
}

/*
 * Function:	Create pair of sockets and put them in desInfoMap
 * Return:		return an integer that indicate if it is successful (0) or not (-1)
 */
int mySocketpair(int domain, int type, int protocol, int des[2]) {
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	int returnVal = socketpair(domain, type, protocol, des);
	if(returnVal != -1) {
        desInfoMap.emplace(des[0], make_shared<socketInfoClass>(des[1]));
        desInfoMap.emplace(des[1], make_shared<socketInfoClass>(des[0]));
	}
	return returnVal;
}

/*
 * Function:	Closing des
 * 		myClose() should not be called until all other calls using the descriptor have finished.
 */
int myClose(int des) {
	{
        lock_guard<shared_mutex> desInfoLk(mapMutex);
        auto iter = desInfoMap.find(des);
        if (iter != desInfoMap.end()) { // if in the map
            auto mySp = iter->second;
            desInfoMap.erase(iter);
            if (mySp) // if shared pointer exists
                return mySp->closing(des);
        }
	}
	return close(des);
}


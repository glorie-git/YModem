/* myioFinal.cpp -- Dec. 12 -- Copyright 2021 Craig Scratchley */
#define AF_LOCAL 1
#define SOCK_STREAM 1
#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <cstring>			// for memcpy ?
#include <atomic>
#include <iostream>
#include "posixThread.hpp" // you are not examined on details of this class

using namespace std;
using namespace std::chrono;

/* Lock-free circular buffer.  This should be threadsafe if one thread is reading
 * and another is writing. */
template<class T>
class CircBuf {
	/* If read_pos == write_pos, the buffer is empty.
	 *
	 * There will always be at least one position empty, as a completely full
	 * buffer (read_pos == write_pos) is indistinguishable from an empty buffer.
	 *
	 * Invariants: read_pos < size, write_pos < size. */
	static const unsigned size = 8 + 1; // capacity of 8 elements
    T buf[size];
	std::atomic<unsigned> read_pos, write_pos;
public:
	CircBuf() {
		read_pos = write_pos = 0;
	}

	/* Write buffer_size elements from buffer into the circular buffer object,
	 * and advance the write pointer.  Return the number of elements that were
	 * able to be written.  If the data will not fit entirely,
	 * as much data as possible will be fit in. */
	unsigned write( const T *buffer, unsigned buffer_size ) {
		T *p[2];
		unsigned sizes[2];

        p[0] = buf + write_pos.load( memory_order_relaxed );
        const unsigned rpos = read_pos.load( memory_order_acquire );
        /* Subtract 1 below, to account for the element that we never fill. */
        if( rpos <= write_pos.load( memory_order_relaxed ) ) {
            // The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data)
            p[1] = buf;
            if (rpos) {
                sizes[0] = size - write_pos.load( memory_order_relaxed );
                sizes[1] = rpos - 1;
            }
            else {
                sizes[0] = size - write_pos.load( memory_order_relaxed ) - 1;
                sizes[1] = rpos; // 0
            }
        } else /* if( rpos > wpos ) */ {
            /* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
            p[1] = nullptr; // could comment out
            sizes[0] = rpos - write_pos.load( memory_order_relaxed ) - 1;
            sizes[1] = 0;
        }

		buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_write_sz=sizes[0]+sizes[1]
		const int from_first = min( buffer_size, sizes[0] );
		memcpy( p[0], buffer, from_first*sizeof(T) );
		if( buffer_size > sizes[0] )
			memcpy(p[1], buffer+from_first, max(buffer_size-sizes[0], 0u)*sizeof(T) );
		write_pos.store( (write_pos.load( memory_order_relaxed ) + buffer_size) % size,
						memory_order_release );
		return buffer_size;
	}

	/* Read buffer_size elements into buffer from the circular buffer object,
	 * and advance the read pointer.  Return the number of elements that were
	 * read.  If buffer_size elements cannot be read, as many elements as
	 * possible will be read */
	unsigned read( T *buffer, unsigned buffer_size ) {
		T *p[2];
		unsigned sizes[2];

        p[0] = buf + read_pos.load( memory_order_relaxed );
        const unsigned wpos = write_pos.load( memory_order_acquire );
        if( read_pos.load( memory_order_relaxed ) <= wpos ) {
            // The buffer looks like "eeeeDDDDeeee" or "eeeeeeeeeeee" (e = empty, D = data)
            p[1] = nullptr; // could comment out
            sizes[0] = wpos - read_pos.load( memory_order_relaxed );
            sizes[1] = 0;
        } else /* if( rpos > wpos ) */ {
            /* The buffer looks like "DDeeeeeeeeDD" (e = empty, D = data). */
            p[1] = buf;
            sizes[0] = size - read_pos.load( memory_order_relaxed );
            sizes[1] = wpos;
        }

		buffer_size = min( buffer_size, sizes[0]+sizes[1]);//max_read_sz=sizes[0]+sizes[1];
		const int from_first = min( buffer_size, sizes[0] );
		memcpy( buffer, p[0], from_first*sizeof(T) );
		if( buffer_size > sizes[0] )
			memcpy( buffer+from_first, p[1], max(buffer_size-sizes[0], 0u)*sizeof(T) );
		read_pos.store( (read_pos.load( memory_order_relaxed ) + buffer_size) % size,
						memory_order_release );
		return buffer_size;
	} /* Original source Copyright (c) 2004 Glenn Maynard. */ 
};  /* See Craig for more details and code before improvements. */ 

namespace{ //Unnamed (anonymous) namespace

    class socketInfoClass;
    typedef shared_ptr<socketInfoClass> socketInfoClassSp;
    map<int, socketInfoClassSp> desInfoMap;

    //	A shared mutex used to protect desInfoMap so only a single thread can modify the map at
    //  a time.  This also means that only one call to functions like mySocketpair() or
    //  myClose() can make progress at a time.  This shared mutex is also used to prevent a
    //  paired socket from being closed at the beginning of a myWrite or myTcdrain function.
    shared_mutex mapMutex;

    class socketInfoClass {
        unsigned totalWritten = 0;
        unsigned maxTotalCanRead = 0;
        condition_variable cvDrain;
        condition_variable cvRead;
        CircBuf<char> circBuffer;
        bool connectionReset = false;
        mutex socketInfoMutex;
    public:
        int pair; 	// Cannot be private because myWrite and myTcdrain using it.
                    // -1 when descriptor closed, -2 when paired descriptor is closed
	socketInfoClass(unsigned pairInit)
	:pair(pairInit) { }

	 // If necessary, make the calling thread wait for a reading thread to drain the data
	int draining(shared_lock<shared_mutex> &desInfoLk) { // operating on object for paired descriptor of original des
		unique_lock<mutex> socketLk(socketInfoMutex);
		desInfoLk.unlock();

		//  once the reader decides the drainer should wakeup, it should wakeup
		if (pair >= 0 && totalWritten > maxTotalCanRead)
			cvDrain.wait(socketLk); // what about spurious wakeup?
		if (pair == -2) {
			errno = EBADF; // check errno
			return -1;
		}
		return 0;
	}

	int writing(int des, const void* buf, size_t nbyte)	{
		// operating on object for paired descriptor
		lock_guard<mutex> socketLk(socketInfoMutex);
		int written = circBuffer.write((const char*) buf, nbyte);
        if (written > 0) {
            totalWritten += written;
            cvRead.notify_one();
        }
		return written;
	}

	int reading2(int des, void * buf, int n, int min, int time)
	{
		int bytesRead;
		unique_lock<mutex> socketLk(socketInfoMutex);

		// would not have got this far if pair == -1
		if ((!maxTotalCanRead && totalWritten >= (unsigned) min) || pair == -2) {
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
			if (time)
                cvRead.wait_for(socketLk, duration<int, deci>{time}, [this, min] {
                    return totalWritten >= (unsigned) min || pair < 0;});
			else
                cvRead.wait(socketLk, [this, min] {
                    return totalWritten >= (unsigned) min || pair < 0;});
            if (pair == -1) { // shouldn't normally happen
                errno = EBADF; // check errno value
                return -1;
            }
            bytesRead = circBuffer.read((char *) buf, n);    
            if (connectionReset && bytesRead == 0) {
                errno = ECONNRESET;
                bytesRead = -1;
                connectionReset = false;
            }            
			if (bytesRead != -1)
				totalWritten -= bytesRead;
            maxTotalCanRead -= n;
			if (totalWritten > 0 || pair < 0) // || pair == -2
				cvRead.notify_one(); // can this affect errno?
		}
		return bytesRead;
	} // .reading2()

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
                des_pair->connectionReset = true;
                cvDrain.notify_all();
            }
            if (maxTotalCanRead > 0) {
                // there shouldn't be any threads waiting in myRead() or myReadcond() on des, but just in case.
                cvRead.notify_all();
            }

			if (des_pair->maxTotalCanRead > 0) {
				// no more data will be written from des
				// notify a thread waiting on reading on paired descriptor
				des_pair->cvRead.notify_one();
			}
			if (des_pair->totalWritten > des_pair->maxTotalCanRead) {
				// there shouldn't be any threads waiting in myTcdrain on des, but just in case.
				des_pair->cvDrain.notify_all();
			}
		}
		// unlock condPairlk here or even above a little bit.
		return 0;
	} // .closing()
	}; // socketInfoClass

    // get shared pointer for des
    socketInfoClassSp getDesInfoP(int des) {
        auto iter = desInfoMap.find(des);
        if (iter == desInfoMap.end())
            return nullptr; // des not in use
        else
            return iter->second; // return the shared pointer
    }
} // unnamed namespace

// see https://developer.blackberry.com/native/reference/core/com.qnx.doc.neutrino.lib_ref
int myReadcond2(int des, void * buf, int n, int min, int time) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	desInfoLk.unlock();
	if (!desInfoP) {
		// not an open "socket" [created with mySocketpair()]
		errno = EBADF; return -1;
	}
	return desInfoP->reading2(des, buf, n, min, time);
}

ssize_t myWrite(int des, const void* buf, size_t nbyte) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	if(desInfoP) {
		if (desInfoP->pair >= 0) {
			// locking mapMutex above makes sure that desinfoP->pair is not closed here
			return desInfoMap[desInfoP->pair]->writing(des, buf, nbyte);
		}
		else {
			errno = EPIPE; return -1;
		}
	}
	errno = EBADF; return -1;
}

int myTcdrain(int des) {
    shared_lock<shared_mutex> desInfoLk(mapMutex);
	socketInfoClassSp desInfoP = getDesInfoP(des);
	if(desInfoP) {
		if (desInfoP->pair == -2)
			return 0; // paired descriptor is closed.
		else { // pair == -1 won't be in *desInfoP now
			// locking mapMutex above makes sure that desinfoP->pair is not closed here
			return desInfoMap[desInfoP->pair]->draining(desInfoLk);
		}
	}
	errno = EBADF; return -1;
}

int mySocketpair(int domain, int type, int protocol, int des[2]) {
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	des[0] = 3; // This is the only function that has materially changed ...
	des[1] = 4; //	... from the code distributed before the exam.
	desInfoMap[des[0]] = make_shared<socketInfoClass>(des[1]);
	desInfoMap[des[1]] = make_shared<socketInfoClass>(des[0]);
	return 0;
}

int myClose(int des) {
	int retVal = 1;
	lock_guard<shared_mutex> desInfoLk(mapMutex);
	auto iter = desInfoMap.find(des);
	if (iter != desInfoMap.end()) { // if in the map
		if (iter->second) // if shared pointer exists
			retVal = iter->second->closing(des);
		desInfoMap.erase(des);
	}
	if (retVal == 1) { // if not-in-use
		errno = EBADF;
		retVal = -1;
	}
	return retVal;
}

const int BSize = 20;

char    B[BSize]; // initially zeroed (filled with NUL characters)
char    B2[BSize]; // initially zeroed (filled with NUL characters)
static int daSktPr[2]; // Descriptor Array for Socket Pair

void coutThreadFunc(void) {
	int RetVal;
	CircBuf<char> buffer;
	//buffer.reserve(5); // note constant of 5
	buffer.write("123456789", 10); // don't forget NUL termination character
	RetVal = buffer.read(B, BSize);
	cout << "Output Line 1 - RetVal: " << RetVal << "  B: " << B << endl;
	cout << "The next line will timeout in 5 or so seconds (50 deciseconds)" << endl;
    RetVal = myReadcond2(daSktPr[1], B, BSize, 10, 50);

    cout << "Output Line 2 - RetVal: " << RetVal << "  B: " << B << endl;
    myWrite(daSktPr[1], "wxyz", 5); // don't forget NUL termination char

	myWrite(daSktPr[0], "ab", 3); // don't forget NUL termination character
    RetVal = myReadcond2(daSktPr[1], B, BSize, 5, 0);

	cout << "Output Line 3 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno);
    else if (RetVal > 0)
        cout << "  B: " << B;
    cout << endl;
    RetVal = myReadcond2(daSktPr[1], B, BSize, 5, 0);
    cout << "Output Line 4 - RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno);
    else if (RetVal > 0)
        cout << "  B: " << B;
    cout << endl;
    RetVal = myReadcond2(daSktPr[1], B, BSize, 5, 0);
    cout << "Output Line 4b- RetVal: " << RetVal;
    if (RetVal == -1)
        cout << " Error:  " << strerror(errno);
    else if (RetVal > 0)
        cout << "  B: " << B;
    cout << endl;
	RetVal = myWrite(daSktPr[0], "123456789", 10); // don't forget NUL termination char
	cout << "Output Line 5 - RetVal: " << RetVal;
	if (RetVal == -1)
		cout << " Error:  " << strerror(errno) << endl;
	else cout << endl;
}

int main() { // debug launch configuration ensures gdb runs at FIFO priority 98 ...
    cpu_set_t cpu_set;
    CPU_SET(0, &cpu_set);
    const char* threadName = "Pri";
    pthread_setname_np(pthread_self(), threadName);
    sched_setaffinity(0, sizeof(cpu_set), &cpu_set); // set processor affinity

    // Pre-allocate some memory for the process.
    // Seems to make priorities work better, at least
    // when using gdb.
    // For some programs, the amount of memory allocated
    // here should perhaps be greater.
    void* ptr = malloc(20000);
    free(ptr);

	try {		// ... realtime policy.
        sched_param sch;
        sch.__sched_priority = 60;
        pthreadSupport::setSchedParam(SCHED_FIFO, sch); //SCHED_FIFO == 1, SCHED_RR == 2
		mySocketpair(AF_LOCAL, SOCK_STREAM, 0, daSktPr);
		myWrite(daSktPr[0], "abc", 3);
		pthreadSupport::posixThread coutThread(SCHED_FIFO, 50, coutThreadFunc); // lower
		myTcdrain(daSktPr[0]);

		myWrite(daSktPr[0], "123", 4); // don't forget NUL termination character
	    myReadcond2(daSktPr[0], B2, 4, 4, 0);

		pthreadSupport::setSchedPrio(40);

		myClose(daSktPr[0]);

		coutThread.join();
		return 0;
	}
	catch (system_error& error) {
		cout << "Error: " << error.code() << " - " << error.what() << '\n';
	}
	catch (...) { throw; }
}

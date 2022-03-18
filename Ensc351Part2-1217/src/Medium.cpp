/*
 * Medium.cpp
 *
 *      Author: Craig Scratchley
 *      Version: September 10, 2021
 *      Copyright(c) 2021 Craig Scratchley
 */

#include <fcntl.h>
#include <unistd.h> // for write()
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "Medium.h"
#include "myIO.h"
#include "VNPE.h"
#include "AtomicCOUT.h"
#include "posixThread.hpp"

#include "PeerY.h"

// Uncomment the line below to turn on debugging output from the medium
//#define REPORT_INFO

//#define SEND_EXTRA_ACKS

//This is the kind medium.

#define T2toT1_CORRUPT_BYTE         395

using namespace std;
using namespace pthreadSupport;

Medium::Medium(int d1, int d2, const char *fname)
:Term1D(d1), Term2D(d2), logFileName(fname)
{
    byteCount = 0;
	ACKforwarded = 0;
	ACKreceived = 0;
	sendExtraAck = false;
    crcMode = false;

	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	logFileD = PE2(myCreat(logFileName, mode), logFileName);
}

Medium::~Medium() {
}

// this function will return false when it detects that the Term2 (sender) socket has closed.
bool Medium::MsgFromTerm2()
{
	blkT bytesReceived; // ?
	int numOfBytesReceived;
	int byteToCorrupt;

	if (!(numOfBytesReceived = PE(myRead(Term2D, bytesReceived, 1)))) {
		COUT << "Medium thread: TERM2's socket closed, Medium terminating" << endl;
		return false;
	}
    byteCount += numOfBytesReceived;

	PE_NOT(myWrite(logFileD, bytesReceived, numOfBytesReceived), numOfBytesReceived);
	//Forward the bytes to Term1 (usually RECEIVER),
	PE_NOT(myWrite(Term1D,   bytesReceived, numOfBytesReceived), numOfBytesReceived);

	if(bytesReceived[0] == CAN) {
	       numOfBytesReceived = PE_NOT(myRead(Term2D, bytesReceived, CAN_LEN - 1), CAN_LEN - 1);
	       // byteCount += numOfBytesReceived;
	       PE_NOT(myWrite(logFileD, bytesReceived, numOfBytesReceived), numOfBytesReceived);
	       //Forward the bytes to Term1 (usually RECEIVER),
	       PE_NOT(myWrite(Term1D,   bytesReceived, numOfBytesReceived), numOfBytesReceived);
	}
    else if (bytesReceived[0] == SOH) {
        if (sendExtraAck) {
    #ifdef REPORT_INFO
                COUT << "{" << "+A" << "}" << flush;
    #endif
            uint8_t buffer = ACK;
            PE_NOT(myWrite(logFileD, &buffer, 1), 1);
            //Write the byte to term2,
            PE_NOT(myWrite(Term2D, &buffer, 1), 1);

            sendExtraAck = false;
        }

        numOfBytesReceived = PE(myRead(Term2D, bytesReceived, (crcMode ? BLK_SZ_CRC : BLK_SZ_CS) - numOfBytesReceived));

        byteCount += numOfBytesReceived;
        if (byteCount >= T2toT1_CORRUPT_BYTE) {
            byteCount = byteCount - T2toT1_CORRUPT_BYTE;  // how large could byteCount end up? (CB - 1) + 133 - CB = 132
            byteToCorrupt = numOfBytesReceived - byteCount; // how small could byteToCorrupt be?
            if (byteToCorrupt < numOfBytesReceived) {
                bytesReceived[byteToCorrupt] = (255 - bytesReceived[byteToCorrupt]);
        #ifdef REPORT_INFO
                COUT << "<" << byteToCorrupt << "x>" << flush;
        #endif
            }
        }

        PE_NOT(myWrite(logFileD, &bytesReceived, numOfBytesReceived), numOfBytesReceived);
        //Forward the bytes to Term1 (RECEIVER),
        PE_NOT(myWrite(Term1D, &bytesReceived, numOfBytesReceived), numOfBytesReceived);
    }
	return true;
}

bool Medium::MsgFromTerm1()
{
	uint8_t buffer[CAN_LEN];
	int numOfByte = PE(myRead(Term1D, buffer, CAN_LEN));
	if (numOfByte == 0) {
		COUT << "Medium thread: TERM1's socket closed, Medium terminating" << endl;
		return false;
	}

    /*note that we record the corrupted ACK in the log file so that we can for it*/
	switch(buffer[0]) {
        case 'C':
            crcMode = true;
            break;
        case CAN:
            crcMode = false;
            break;
        case EOT:
            crcMode = false;
            break;
        case ACK: {
            ACKreceived++;

            if((ACKreceived%10)==0)
            {
                ACKreceived = 0;
                buffer[0]=NAK;
    #ifdef REPORT_INFO
                COUT << "{" << "AxN" << "}" << flush;
    #endif
            }
    #ifdef SEND_EXTRA_ACKS
            else/*actually forwarded ACKs*/
            {
                ACKforwarded++;

                if((ACKforwarded%6)==0)/*Note that this extra ACK is not an ACK forwarded from receiver to the sender, so we don't increment ACKforwarded*/
                {
                    ACKforwarded = 0;
                    sendExtraAck = true;
                }
            }
    #endif
        }
	}

	PE_NOT(write(logFileD, buffer, numOfByte), numOfByte);

	//Forward the buffer to term2,
	PE_NOT(myWrite(Term2D, buffer, numOfByte), numOfByte);
	return true;
}

void
Medium::
mediumFuncT1toT2()
{
    PE_0(pthread_setname_np(pthread_self(), "M1to2"));
    while (MsgFromTerm1());
}

void Medium::run()
{
//    posixThread mediumThrd1to2(SCHED_FIFO, 35, &Medium::mediumFuncT1toT2, this); // gives error
    thread mediumThrd1to2(&Medium::mediumFuncT1toT2, this);
    pthreadSupport::setSchedPrio(45); // raise priority up slightly.  FIFO?

    //transfer data from Term2 (sender)
    while (MsgFromTerm2());

    mediumThrd1to2.join(); 
	PE(myClose(logFileD));
	PE(myClose(Term1D));
	PE(myClose(Term2D));
}


void mediumFunc(int T1d, int T2d, const char *fname)
{
    PE_0(pthread_setname_np(pthread_self(), "M2to1"));
    Medium medium(T1d, T2d, fname);
    medium.run();
}


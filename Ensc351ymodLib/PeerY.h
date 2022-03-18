/*
 * PeerY.h
 *
 *      Author: wcs
 */

#ifndef PEERY_H_
#define PEERY_H_

#include <cstdint> // for uint8_t
#include <time.h>
#include <sstream>
#include <memory>
#include <string>

#include "ss_api.hxx"
#include "crc.h"

//#define CHUNK_SZ	 128
#define SOH_OH  	 1			//SOH Byte Overhead
#define BLK_NUM_AND_COMP_OH  2	//Overhead for blkNum and its complement
#define DATA_POS  	 (SOH_OH + BLK_NUM_AND_COMP_OH)	//Position of data in buffer
#define PAST_CHUNK 	 (DATA_POS + CHUNK_SZ)		//Position of checksum in buffer

#define CS_OH           1			                    //Overhead for CheckSum
#define REST_BLK_OH_CS  (BLK_NUM_AND_COMP_OH + CS_OH)	//Overhead in rest of block
#define REST_BLK_SZ_CS  (CHUNK_SZ + REST_BLK_OH_CS)
#define BLK_SZ_CS  	 	(SOH_OH + REST_BLK_SZ_CS)
#define MOST_BLK_SZ_CS	(BLK_SZ_CS - 1)

#define CRC_OH           2			                    //Overhead for CRC16
#define REST_BLK_OH_CRC  (BLK_NUM_AND_COMP_OH + CRC_OH)	//Overhead in rest of block
#define REST_BLK_SZ_CRC  (CHUNK_SZ + REST_BLK_OH_CRC)
#define BLK_SZ_CRC  	 (SOH_OH + REST_BLK_SZ_CRC)
#define MOST_BLK_SZ_CRC	 (BLK_SZ_CRC - 1)

#define GLITCH_SPACE  30			//Space for extra glitch bytes
#define BUF_SZ  (BLK_SZ_CRC + GLITCH_SPACE)

#define CAN_LEN 8 // was 2 // the number of CAN characters to send to cancel a transmission

#define errB 10 // the error bound.  The number of errors in a row that are allowed.

#define CANC_C	"&c\n" // string for cancel command

// define names for control characters used in the protocol.
#define SOH 0x01
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15
#define CAN 0x18 // 24  
#define	CTRL_Z 0x1A //	26

#define FAST_SIM

// timeouts in units
#ifdef FAST_SIM
#define TM_SOH_C (.5*UNITS_PER_SEC) // timeout waiting for SOH/EOT (normally 3 seconds)
//#define TM_END (.5*UNITS_PER_SEC) // timeout waiting for SOH/EOT (normally 3 seconds)
#define TM_SOH (.9*UNITS_PER_SEC) // timeout waiting for SOH/EOT -- should normally be 10 seconds
#define TM_VL  (15*UNITS_PER_SEC) // Very long timeout -- normally 60 seconds
#define TM_2CHAR (.4*UNITS_PER_SEC) // normally wait for more than 1 second (1 second plus)
#define TM_CHAR (.2*UNITS_PER_SEC) // normally wait for 1 second
#else
#define TM_SOH_C (3*UNITS_PER_SEC) // timeout waiting for SOH/EOT (3 seconds)
#define TM_END (3*UNITS_PER_SEC) // timeout waiting for SOH/EOT (3 seconds)
#define TM_SOH (10*UNITS_PER_SEC) // timeout waiting for SOH/EOT (10 seconds)
#define TM_VL  (60*UNITS_PER_SEC) // Very long timeout (60 seconds)
#define TM_2CHAR (2*UNITS_PER_SEC) // wait for more than 1 second (1 second plus)
#define TM_CHAR (1*UNITS_PER_SEC) // wait for 1 second
#endif

#define UNITS_PER_SEC 10 // deciseconds (or tenths of seconds)

#define MILLION 1000000

#define dSECS_PER_UNIT (10/UNITS_PER_SEC)  		//deciseconds per unit
#define mSECS_PER_UNIT (1000/UNITS_PER_SEC)		//milliseconds per unit
#define uSECS_PER_UNIT (MILLION/UNITS_PER_SEC) 	//microseconds per unit

typedef uint8_t blkT[BLK_SZ_CRC]; // blkT is the the type for a block

enum {CONT, //Continue event
	SER, 	//Event from serial port
	TM, 	//Timeout event
	KB_C	//Cancellation via Keyboard event
};

//void 
//crc16ns (uint16_t* crc16nsP, uint8_t* buf);

#define CON_OUT(fd, x) { \
	std::ostringstream ost; \
	ost << x; \
	{ \
		/* if (fd == STDIN_FILENO) ScopedMutex(&consoleMutex);*/ \
		myWrite (fd, ost.str().c_str(), strlen(ost.str().c_str())); \
	} \
}

class PeerY {
public:
	PeerY(int d, char left, char right, const char *smLogN, int conInD, int conOutD)
	;

	void tm(int timeoutUnits);
	void tmRed(int reductionUnits);
	void tmPush(int timeoutUnits);
	void tmPop();

	//Send a byte to the remote peer across the medium
	void
	sendByte(uint8_t byte)
	;

	std::string result;  // the result of the file transfer
	unsigned errCnt;	 // counts the number of "errors" in a row
    int transferringFileD = -1;  // descriptor for file being read from or written to.

	/* A variable that records the fact that a keyboard cancel event
	 * has been received.
	 */
	bool KbCan = false;


protected:
	void
	transferCommon(std::shared_ptr<smartstate::StateMgr> mySM, bool reportInfoParam)
	;

	int mediumD; // descriptor for serial port or delegate

	char logLeft; // for this peer, symbol to use to start a phrase of logging information
	char logRight; // symbol to use to end info phrase for this peer
	const char *smLogName; // name for optional statechart logging file

	int consoleInId;	// console input descriptor for Xmodem transfer
	int consoleOutId;	// console output descriptor for Xmodem transfer

private:
	bool reportInfo; // should debugging information be reported

	long long int absoluteTimeout;  // time in microseconds, after peer was constructed, of timeout
	long long int holdTimeout;		// hold original timeout during temporary timeout.

	/*_CSTD*/ time_t sec_start;		// The time, as the number of seconds, when the peer was constructed
	long long int  elapsed_usecs()
	;
};

#endif /* PEERY_H_ */

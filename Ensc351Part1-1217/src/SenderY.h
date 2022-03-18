#ifndef SENDER_H
#define SENDER_H

#include <unistd.h>
#include <vector>

#include "PeerY.h"

class SenderY : public PeerY
{
	friend
	void testSenderY(std::vector<const char*> iFileNames, int mediumD)
	;

public:
	SenderY(std::vector<const char*> iFileNames, int d);
	void statBlk(const char* fileName);
	//void sendBlk(blkT blkBuf);
	void sendBlk(uint8_t blkBuf[BLK_SZ_CRC]);
	void sendFiles();
    void cans();
	ssize_t bytesRd;  // The number of bytes last read from the input file.

private:
	std::vector<const char*> fileNames;
	unsigned fileNameIndex;
	uint8_t blkBuf[BLK_SZ_CRC];     // a  block
	// blkT blkBuf;    // A block // causes inability to debug this array.
	uint8_t blkNum;	// number of the current block to be acknowledged
	//void genBlk(blkT blkBuf);
	void genBlk(uint8_t blkBuf[BLK_SZ_CRC]);
	//void genStatBlk(blkT blkBuf, const char* fileName);
	void genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
	;
};

#endif

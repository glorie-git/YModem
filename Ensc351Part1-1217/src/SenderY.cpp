//============================================================================
//
//% Student Name 1: student1
//% Student 1 #: 123456781
//% Student 1 userid (email): stu1 (stu1@sfu.ca)
//
//% Student Name 2: student2
//% Student 2 #: 123456782
//% Student 2 userid (email): stu2 (stu2@sfu.ca)
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
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderY.cpp
// Version     : September 3rd, 2021
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "SenderY.h"

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <errno.h>
#include <fcntl.h>	// for O_RDWR
#include <sys/stat.h>
#include <string> // for to string

#include "myIO.h"

using namespace std;

SenderY::
SenderY(vector<const char*> iFileNames, int d)
:PeerY(d),
 bytesRd(-1), 
 fileNames(iFileNames),
 fileNameIndex(0),
 blkNum(0)
{
}

//-----------------------------------------------------------------------------

/* generate a block (numbered 0) with filename and filesize */
//void SenderY::genStatBlk(blkT blkBuf, const char* fileName)
void SenderY::genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
{
    // ********* additional code must be written ***********
        //int strLen = strlen(fileName);

        struct stat st;
        stat(fileName, &st);
        unsigned fileSize = st.st_size;
        //string fileSizeChar = std::to_string(st.st_size);

        string subFileName = fileName;
        subFileName = subFileName.substr(subFileName.find_last_of("/\\")+1);

        char fileNameChr [subFileName.length() + 1];
        strcpy(fileNameChr, subFileName.c_str());

        memset(blkBuf,0, CHUNK_SZ);
        memset(blkBuf,SOH,1);
        memset(&blkBuf[1], (int)blkNum, 1);
        memset(&blkBuf[2], ~blkNum, 1);

        int pos = snprintf((char *)&blkBuf[3], strlen(subFileName.c_str()) + 1, subFileName.c_str() );
        snprintf((char *)&blkBuf[pos + DATA_POS + 1], sizeof(st.st_size), (const char*)fileSize);


    /* calculate and add CRC in network byte order */
    // ********* The next couple lines need to be changed ***********

    crc16ns((uint16_t *)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
*/
//void SenderY::genBlk(blkT blkBuf)
void SenderY::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    // ********* The next line needs to be changed ***********
    if (-1 == (bytesRd = myRead(transferringFileD, &blkBuf[DATA_POS], CHUNK_SZ )))
        ErrorPrinter("myRead(transferringFileD, &blkBuf[0], CHUNK_SZ )", __FILE__, __LINE__, errno);
    // ********* and additional code must be written ***********

    /* calculate and add CRC in network byte order */
    // ********* The next couple lines need to be changed ***********
    //uint16_t myCrc16ns;
    uint16_t myCrc16ns = (uint16_t )blkBuf[PAST_CHUNK];
    crc16ns(&myCrc16ns, &blkBuf[blkNum]);

    // ...
}

void SenderY::cans()
{
    // ********* send CAN_LEN of CAN characters ***********
    //int canLen = 2;
    char buffer [CAN_LEN];
    memset(buffer, CAN, CAN_LEN);
    myWrite(mediumD, buffer, CAN_LEN );
}

//uint8_t SenderY::sendBlk(blkT blkBuf)
void SenderY::sendBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    // ********* fill in some code here to send a block (write to mediumD) ***********
    myWrite(mediumD, &blkBuf[blkNum], CHUNK_SZ);
}

void SenderY::statBlk(const char* fileName)
{
    blkNum = 0;
    // assume 'C' received from receiver to enable sending with CRC
    genStatBlk(blkBuf, fileName); // prepare 0eth block
    sendBlk(blkBuf); // send 0eth block
    // assume sent block will be ACK'd
}

void SenderY::sendFiles()
{
    for (auto fileName : fileNames) {
        // const char* fileName = fileNames[fileNameIndex];
        transferringFileD = myOpen(fileName, O_RDWR, 0);
        if(transferringFileD == -1) {
            // ********* fill in some code here to write 8 CAN characters ***********
            cans();
            cout /* cerr */ << "Error opening input file named: " << fileName << endl;
            result = "OpenError";
            return;
        }
        else {
            cout << "Sender will send " << fileName << endl;

            // do the protocol, and simulate a receiver that positively acknowledges every
            //	block that it receives.

            statBlk(fileName);

            // assume 'C' received from receiver to enable sending with CRC
            genBlk(blkBuf); // prepare 1st block
            while (bytesRd)
            {
                blkNum ++; // 1st block about to be sent or previous block was ACK'd

                sendBlk(blkBuf); // send block

                // assume sent block will be ACK'd
                genBlk(blkBuf); // prepare next block
                // assume sent block was ACK'd
            };
            // finish up the file transfer, assuming the receiver behaves normally and there are no transmission errors
            // ********* fill in some code here ***********
            int eotLen = 2;
            char buffer [eotLen];
            memset(buffer, EOT, eotLen);
            myWrite(mediumD, buffer, eotLen );

            //(myClose(transferringFileD));
            if (-1 == myClose(transferringFileD))
                ErrorPrinter("myClose(transferringFileD)", __FILE__, __LINE__, errno);
        }
    }
    // indicate end of the batch.
    statBlk("");

    result = "Done";
}


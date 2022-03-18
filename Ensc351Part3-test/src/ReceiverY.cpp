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
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : ReceiverY.cpp
// Version     : September 24th, 2021
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "ReceiverY.h"

#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include "myIO.h"
#include "VNPE.h"

//using namespace std;

ReceiverY::
ReceiverY(int d)
:PeerY(d),
 closedOk(1),
 anotherFile(0xFF),
NCGbyte('C'),
goodBlk(false), 
goodBlk1st(false), 
syncLoss(false), // transfer will end when syncLoss becomes true
numLastGoodBlk(0)
{
}

/* Only called after an SOH character has been received.
The function receives the remaining characters to form a complete
block.
The function will set or reset a Boolean variable,
goodBlk. This variable will be set (made true) only if the
calculated checksum or CRC agrees with the
one received and the received block number and received complement
are consistent with each other.
Boolean member variable syncLoss will only be set to
true when goodBlk is set to true AND there is a
fatal loss of syncronization as described in the XMODEM
specification.
The member variable goodBlk1st will be made true only if this is the first
time that the block was received in "good" condition. Otherwise
goodBlk1st will be made false.
*/
void ReceiverY::getRestBlk()
{
    // ********* this function must be improved ***********

    uint8_t currAt = rcvBlk[1],
            lastGood = numLastGoodBlk;

    PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);

    goodBlk1st = goodBlk = syncLoss = false;

    uint16_t rcvCrc, calcCrc;
    crc16ns(&calcCrc, &rcvBlk[DATA_POS]);

    rcvCrc = (*(uint16_t*)&rcvBlk[PAST_CHUNK]);

    if ( rcvBlk[2] == (255 - rcvBlk[1]) && rcvCrc == calcCrc) {
        goodBlk = true;

//      There is a fatal loss of sync when if received block number is not what is expected
//      or if the block number is not the same as numLastGoodBlk
        if (!((uint8_t)rcvBlk[1] == ++lastGood || (uint8_t)rcvBlk[1] == numLastGoodBlk)){
            syncLoss = true;
        }
        numLastGoodBlk = rcvBlk[1];
    }
    if( numLastGoodBlk != --currAt && goodBlk ) {//rcvBlk[1] && goodBlk ) {
        goodBlk1st = true;
    }

}

//Write chunk (data) in a received block to disk
void ReceiverY::writeChunk()
{
    PE_NOT(myWrite(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}

int
ReceiverY::
openFileForTransfer()
{
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    transferringFileD = myCreat((const char *) &rcvBlk[DATA_POS], mode);
    return transferringFileD;
}

int
ReceiverY::
closeTransferredFile()
{
    closedOk = myClose(transferringFileD);
    return closedOk;
}

//Send CAN_LEN CAN characters in a row to the XMODEM sender, to inform it of
//  the cancelling of a file transfer
void ReceiverY::cans()
{
    // no need to space in time CAN chars coming from receiver
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}

uint8_t ReceiverY::checkForAnotherFile() {
    return (anotherFile = rcvBlk[DATA_POS]);
}

void ReceiverY::receiveFiles()
{
    ReceiverY& ctx = *this; // needed to work with SmartState-generated code

    // ***** improve this member function *****

    // below is just an example template.  You can follow a
    //  different structure if you want.

    while (
            sendByte(ctx.NCGbyte),
            PE_NOT(myRead(mediumD, rcvBlk, 1), 1), // Should be SOH
            ctx.getRestBlk(), // get block 0 with fileName and filesize
            checkForAnotherFile()) {

        if(openFileForTransfer() == -1) {
            cans();
            result = "CreatError"; // include errno or so
            return;
        }
        else {
            sendByte(ACK); // acknowledge block 0 with fileName.

            // inform sender that the receiver is ready and that the
            //      sender can send the first block
            sendByte(ctx.NCGbyte);

            while(PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH))
            {
                ctx.getRestBlk();
                if (ctx.goodBlk1st) {
                    ctx.errCnt = 0;
                    ctx.anotherFile = 0;
                } else {
                    ctx.errCnt++;
                }

                if (!ctx.syncLoss && (ctx.errCnt < errB)) {
                    if (ctx.goodBlk) {
                        ctx.sendByte(ACK);
                        if (ctx.anotherFile) {
                            ctx.sendByte(ctx.NCGbyte);
                        }
                    } else {
                        ctx.sendByte(NAK);
                    }
                    if (ctx.goodBlk1st) {
                        ctx.writeChunk();
                    }
                }

                if (ctx.syncLoss || ctx.errCnt >= errB) {
                    ctx.cans();
                    ctx.closeTransferredFile();
                    if (ctx.syncLoss) {
                        ctx.result = "LossOfSynchronization";
                    } else {
                        ctx.result = "ExcessiveErrors";
                    }
                }

            };
            // check if a EOT was just read in the condition for the while loop
            if (rcvBlk[0] == EOT) {
                ctx.sendByte(NAK); // NAK the first EOT
                PE_NOT(myRead(mediumD, rcvBlk, 1), 1);
                if (rcvBlk[0] == EOT){
                    ctx.sendByte(ACK); // ACK for second EOT
                    ctx.closeTransferredFile();
                }
            }

            // Check if the file closed properly.  If not, result should be "CloseError".
            if (ctx.closedOk) {
                ctx.cans();
                ctx.result = "ClosedError";
            }
        }
    }
    if (rcvBlk[0] == CAN) {
        if(transferringFileD != -1){
            ctx.closeTransferredFile();
        }
        ctx.result = "SndCancelled";
    }
    if (rcvBlk[CHUNK_SZ+DATA_POS+1] == NAK && !ctx.anotherFile){
        ctx.result = "Done";
    }
}

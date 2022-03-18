/*
 * crc.h
 *
 *  Created on: Nov 16, 2020
 *      Author: osboxes
 */

#ifndef CRC_H_
#define CRC_H_

#include <stdint.h>

#define CHUNK_SZ     128

#ifdef __cplusplus
extern "C"
{
#endif

// Should return via crc16nsP a crc16 in 'network byte order'.
// Derived from code in "rbsb.c" (see above).
// Line comments in function below show lines removed from original code.
void crc16ns (uint16_t* crc16nsP, uint8_t* buf);

#ifdef __cplusplus
}
#endif

#endif /* CRC_H_ */

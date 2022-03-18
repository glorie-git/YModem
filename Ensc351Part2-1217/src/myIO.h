#ifndef MYIO_H_
#define MYIO_H_

#include <unistd.h> 	// for size_t
#include <sys/stat.h>	// for mode_t

int myOpen(const char *pathname, int flags, ...) //, mode_t mode)
;

int myCreat(const char *pathname, mode_t mode);
int mySocketpair( int domain, int type, int protocol, int des_array[2] );
ssize_t myRead( int des, void* buf, size_t nbyte );
ssize_t myWrite( int des, const void* buf, size_t nbyte );
int myClose(int des);

// The last two are not ordinarily used with sockets
int myTcdrain(int des); //is also included for purposes of the course.

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
int myReadcond(int des, void * buf, int n, int min, int time, int timeout);

#endif /*MYIO_H_*/

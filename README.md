# YModem

ENSC 351: Real
time and Embedded
Systems
Craig Scratchley, Fall 2021

Multipart Project Part #1

This part of the project does not require understanding real-time or embedded systems. It is a standard programming assignment and allows you to practice programming using relatively simple C++.

For the first part of the project, instead of sending the blocks and any other characters over a serial port or similar, the blocks and other characters should simply be written to an output file, “ymodemSenderData.dat”. This file should only contain bytes that would normally be sent from the sender to the receiver. We are just imagining that a receiver exists and are assuming that it will correctly start the sender, positively acknowledge every block that it receives, and properly handle the termination of each transfer after it has received all the blocks for each file in the batch. A YMODEM sender implementation is allowed to send blocks that can hold either 128 bytes or 1024 bytes of data. At least for this first part of the project, we will generate only blocks that can hold just 128 bytes of data.

I have written a rather complete template for you to modify. You will only need to modify the member functions genStatBlk, genBlk(), sendBlk(), cans(), sendFiles(), and crc16ns(), calling any additional functions that you might want. See the sendFile() member function as given to you for a simulation of the main part of the protocol in operation and for the use of the genBlk() member function (you will need to finish the sendFile() member function). Use the myCreat(), myOpen(), myRead(), myWrite(), and myClose() wrapper functions for the POSIX functions creat(), open(), read(), write(), and close() to create, open, read from, write to, and close files (and, later in the course, devices like serial ports). You can find
documentation on POSIX functions online and on the Xubuntu Virtual Machine that I created for the class. We will be using Linux this term, and I recommend that everybody use the Xubuntu VM that I have created.

The template already makes some use of the wrappers for these POSIX functions. Mac OS X and Linux support POSIX functions “out-of-the-box”. Notice that in my template all direct or indirect calls to the POSIX functions open(), create(), read(), write(), and close() are checked for a return value like –1, indicating that an error occurred or something else went wrong during execution of the function. For any direct or indirect calls to write() and/or read() that you need to add to the template, you must handle a return value of -1 in a suitable way, and also handle an unexpected return value where reasonable to do so. Do not modify lines of code in my template unless modifications to those lines are indicated or you explain in a comment why you have decided to make such modifications. Modifications that you feel improve the code are encouraged but otherwise modifications are discouraged as, among other things, they may affect marking. Discuss with me if you want to make modifications where not indicated.
Assume that the imaginary receiver sends a ‘C’ to begin the transfer of file information or file contents and always sends an ACK for each block. After the last block of a file is ACKed, send an EOT, which will be NAKed, and then repeat the EOT. So for a single file that needs 4 blocks and has no transmission errors an imaginary receiver requesting CRCs (and being provided information on the file) would send ‘C’, ACK, ‘C’, ACK, ACK, ACK, ACK, NAK, ACK, ‘C’, ACK.

Except where indicated in the code, for now don’t worry about references to the CAN byte in the specification, and don’t worry about timing. Just assume that, for example, an ACK will be quickly received after each block is sent.

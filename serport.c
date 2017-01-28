// serport.c - Serial Port Handler

// Permission is hereby granted to freely copy,
// modify, utilize and distribute this example in
// whatever manner you desire without restriction.

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>

#include "serport.h"

// Used to reset port opts on close
static struct termios saved_opts;

// opens the serial port
// return code:
//   > 0 = fd for the port
//   -1 = open failed
int OpenSerPort(char* sDeviceName)
{
    char sPortName[32];
    int fd = -1;
    int nMode = O_RDWR | O_NOCTTY;
    struct termios serial_opts;

    sprintf(sPortName, "/dev/tty%s", sDeviceName);

    fd = open(sPortName, nMode);
    if (fd < 0)
    {
        printf("open error %d %s\n", errno, strerror(errno));
        return fd;
    }
    // Current config
    tcgetattr(fd, &serial_opts);

    // Restore on close
    saved_opts = serial_opts;

    cfmakeraw(&serial_opts);

    cfsetospeed(&serial_opts, B19200);
    cfsetispeed(&serial_opts, B19200);

    serial_opts.c_cflag &= ~CSTOPB;
    serial_opts.c_cflag &= ~CRTSCTS;
    serial_opts.c_cflag |= (CLOCAL | CREAD);

    // set new config
    tcsetattr(fd, TCSANOW, &serial_opts);

    return fd;
}

// writes counted string to the serial port
// return code:
//   >= 0 = number of characters written
//   -1 = write failed
int WriteSerPort(int fd, unsigned char* psOutput, int nCount)
{
    int iOut;

    if (fd < 1)
    {
        printf(" port is not open\n");
        return -1;
    }

    iOut = write(fd, psOutput, nCount);
    if (iOut < 0)
    {
        printf("write error %d %s\n", errno, strerror(errno));
    }

    if (iOut != nCount)
        printf("Write incomplete!\n");

    return iOut;
}

// read string from the serial port
// return code:
//   >= 0 = number of characters read
//   -1 = read failed
int ReadSerPort(int fd, unsigned char* psResponse, int iMax, unsigned char cCmd)
{
    int nfd, nRet, nRead;
    fd_set  infds;
    struct timeval tmo;
    int nAvailable;

    if (fd < 1)
    {
        printf(" port is not open\n");
        return -1;
    }

    // Init vars
    nRead = 0;
    FD_ZERO(&infds);

    while (1)
    {
        tmo.tv_sec = 3;
        tmo.tv_usec = 0;
        FD_SET(fd, &infds);

        nfd = select(fd + 1, &infds, NULL, NULL, &tmo);
        if (nfd <= 0)
        {
            if (nfd == 0)
            {
                printf("timeout - %d read\n", nRead);
                return -(nRead + 1);
            }
            else
            {
                printf("Select error %d %s\n", errno, strerror(errno));
                return -1;
            }
        }

        // Determin bytes to read (avoid blocking)
        if (ioctl(fd, FIONREAD, &nAvailable))
        {
            printf("ioctl failed %d, %s\n", errno, strerror(errno));
            return -1;
        }

        if (nAvailable == 0)
            continue;

        // Check buffer overrun
        if (nAvailable > (iMax - nRead))
        {
            printf("Serial buffer overflow. Available = %d, buffer = %d\n", nAvailable, iMax - nRead);
            return -1;
        }
        // Read some bytes
        nRet = read(fd, &psResponse[nRead], nAvailable);
        if (nRet < 0)
        {
            printf("Read error %d, %s -- %d so far\n", errno, strerror(errno), nRead);
            return -1;
        }
        // nRet may be 0
        nRead += nRet;
        if (nRead >= iMax)
            break;
    }

    // Check cmd echo
    if ((cCmd != 0) && (psResponse[0] != cCmd))
    {
        printf("*** Cmd '%c' (0x%02X) not equal 0x%02X\n", cCmd, cCmd, psResponse[0]);
        return -1;
    }

    return iMax;
}

// closes the serial port
void CloseSerPort(int fd)
{
    if (fd > 0)
    {
        tcflush(fd, TCIOFLUSH);
        tcsetattr(fd, TCSANOW, &saved_opts);
        close(fd);
    }
}

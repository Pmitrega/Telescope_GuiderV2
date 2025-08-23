#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include "UartHandler.hpp"
#include <iostream>

int initUart()
{
    const char device[] = "/dev/ttyS5";
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0)
    {
        perror("open");
        return 1;
    }
    std::cout << "initalizing UART" << std::endl;
    struct termios tty;
    tcgetattr(fd, &tty);

    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8 bits
    tty.c_cflag &= ~PARENB;                     // no parity
    tty.c_cflag &= ~CSTOPB;                     // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                    // no flow control
    tty.c_cflag |= CREAD | CLOCAL;              // enable read, ignore ctrl lines

    tty.c_lflag = 0; // raw mode
    tty.c_oflag = 0;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    tcsetattr(fd, TCSANOW, &tty);

    write(fd, "Hello UART\n", 11);

    close(fd);
    return 0;
}
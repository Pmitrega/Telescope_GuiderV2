#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include "UartHandler.hpp"
#include <iostream>


#define STPPS_TO_SECPS(STEP_PER_SEC) ((int)(STEP_PER_SEC * 144.f / 15.f))

int UartHandler::initUart()
{
    const char device[] = "/dev/ttyS5";
    m_uart_fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (m_uart_fd < 0)
    {
        perror("open");
        return 1;
    }
    std::cout << "initalizing UART" << std::endl;
    struct termios tty;
    tcgetattr(m_uart_fd, &tty);

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

    tcsetattr(m_uart_fd, TCSANOW, &tty);

    write(m_uart_fd, "Hello UART\n", 11);
    
    return 0;
}
UartHandler::~UartHandler(){
    if(m_uart_fd != -1){
        close(m_uart_fd);
    }
}

void UartHandler::setRaSpeed(float ra_speed){
    int mess_len;
    mess_len = sprintf(m_tx_buffer, "-R%d\n", STPPS_TO_SECPS(ra_speed));
    int ret = write(m_uart_fd, m_tx_buffer, mess_len);
    if(ret == -1){
        std::cerr<< "Failed to send uart message" << std::endl;
        return;
    }
    std::cout<< "setting Ra speed " << m_tx_buffer << std::endl;
}

void UartHandler::setDecSpeed(float dec_speed){
    int mess_len;
    mess_len = sprintf(m_tx_buffer, "-D%d\n", STPPS_TO_SECPS(dec_speed));
    write(m_uart_fd, m_tx_buffer, mess_len);
    int ret = write(m_uart_fd, m_tx_buffer, mess_len);
    if(ret == -1){
        std::cerr<< "Failed to send uart message" << std::endl;
        return;
    }
    std::cout<< "setting Dec speed " << m_tx_buffer << std::endl;
}
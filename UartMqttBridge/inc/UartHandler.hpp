#ifndef UART_HANDLER_HPP
#define UART_HANDLER_HPP

class UartHandler{
public:
    ~UartHandler();
    int initUart();
    void setRaSpeed(float speed_sps);
    void setDecSpeed(float speed_sps);
private:
    int m_uart_fd = -1;
    char m_tx_buffer[256];
    char m_rx_buffer[256];
};

#endif
#ifndef SPI_H
#define SPI_H

#include "Core.h"

constexpr uint16_t SPITimeoutMillis = 250;
typedef enum
{
    SPI_ERROR = -1,
    SPI_OK = 0,
    SPI_ERROR_TIMEOUT = 1,
    SPI_ERROR_ARGUMENT,
    SPI_ERROR_OVERRUN,
    SPI_ERROR_MODE_FAULT,
    SPI_ERROR_OVERRUN_AND_MODE_FAULT
} spi_status_t;

class SPI
{
public:
    virtual void configureDevice(uint32_t bits, uint32_t clockMode, uint32_t bitRate);
    virtual spi_status_t transceivePacket(const uint8_t *tx_data, uint8_t *rx_data, size_t len);    
    virtual bool waitForTxEmpty();
    virtual void initPins(Pin sck, Pin miso, Pin mosi, Pin cs = NoPin);
    static SPI *getSSPDevice(SSPChannel channel);
};




#endif

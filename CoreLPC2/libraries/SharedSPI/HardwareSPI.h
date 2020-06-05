#ifndef HARDWARESPI_H
#define HARDWARESPI_H

#include "Core.h"
#include "SharedSpi.h"
#include "chip.h"
#include "variant.h"
#include "SPI.h"
#include "DMA.h"

#include "FreeRTOS.h"
#include "task.h"

extern "C" void SSP0_IRQHandler(void);
extern "C" void SSP1_IRQHandler(void);

class HardwareSPI;
typedef void (*SPICallbackFunction)(HardwareSPI *spiDevice) noexcept;
class HardwareSPI: public SPI
{
public:
    HardwareSPI(LPC_SSP_T* sspDevice, Pin* spiPins);
    spi_status_t sspi_transceive_packet(const uint8_t *tx_data, uint8_t *rx_data, size_t len);
    void setup_device(const struct sspi_device *device);
    bool waitForTxEmpty();
    void configureDevice(uint32_t deviceMode, uint32_t bits, uint32_t clockMode, uint32_t bitRate, bool hardwareCS);
    void disable();
    void startTransfer(const uint8_t *tx_data, uint8_t *rx_data, size_t len, SPICallbackFunction ioComplete);
    void InitPins(Pin sck, Pin miso, Pin mosi, Pin cs);

    static HardwareSPI SSP0;
    static HardwareSPI SSP1;

private:
    LPC_SSP_T* ssp;
    bool needInit;
    Pin * const pins;
    SPICallbackFunction callback;
    TaskHandle_t waitingTask;

    void configurePins(bool hardwareCS);
    void configureMode(uint32_t deviceMode, uint32_t bits, uint32_t clockMode, uint32_t bitRate);
    void configureBaseDevice();
    friend void transferComplete(HardwareSPI *spiDevice) noexcept;
    friend void SSP0_IRQHandler(void);
    friend void SSP1_IRQHandler(void);
};

#endif

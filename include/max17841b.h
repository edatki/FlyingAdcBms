#ifndef MAX17841B_H
#define MAX17841B_H

#include <stdint.h>
#include "digio.h"

class MAX17841B
{
    public:
        MAX17841B(uint32_t _spi_interface, DigIo *_cs_pin) : spi_interface(_spi_interface), cs_pin(_cs_pin) {};

        uint8_t ReadRegister(uint8_t regAddress);
        void WriteRegister(uint8_t regAddress, uint8_t value);
        void LoadTransmitQueue(uint8_t *data, uint8_t length);
        void ReadReceiveQueue(uint8_t *data, uint8_t length);
        bool CheckReceiveBufferError();
        uint16_t ReadAddressedSlave(uint8_t dataRegister, uint8_t address, bool setupDone);
        void WriteAddressedSlave(uint8_t dataRegister, uint16_t data, uint8_t address, bool setupDone);
    
    private:
        uint32_t spi_interface;
        DigIo *cs_pin;
};



#endif // MAX17841B_H

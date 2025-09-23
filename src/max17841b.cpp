#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "digio.h"
#include "hwdefs.h"
#include "max17841b.h"

#define CMD_HELLO_ALL 0x57
#define CMD_WRITE_ALL 0x02
#define CMD_READ_ALL 0x03
#define CMD_WRITE_DEVICE 0x04
#define CMD_READ_DEVICE 0x05

#define WR_RX_INT_FLAGS 0x08
#define RD_RX_INT_FLAGS 0x09
#define WR_LD_Q 0xC0
#define RD_NXT_MSG 0x93

#define MAX_MODULES 32
#define MAX_RX_MESSAGE_LENGTH (5 + (2 * MAX_MODULES))



uint8_t MAX17841B::ReadRegister(uint8_t regAddress) {
    this->cs_pin->Clear();
    spi_xfer(SPI2, regAddress);
    uint8_t value = spi_xfer(SPI2, regAddress);
    this->cs_pin->Set();
    return value;
}

void MAX17841B::WriteRegister(uint8_t regAddress, uint8_t value) {
}

void MAX17841B::LoadTransmitQueue(uint8_t *data, uint8_t length)
{
    //cs low
    spi_xfer(this->spi_interface, WR_LD_Q);
    spi_xfer(this->spi_interface, length);
    for (uint8_t i = 0; i < length; i++) spi_xfer(this->spi_interface, data[i]);
    //cs high
}

void MAX17841B::ReadReceiveQueue(uint8_t *data, uint8_t length) 
{
    //cs low
    spi_xfer(this->spi_interface, RD_NXT_MSG);
    for (uint8_t i = 0; i < length; i++) data[i] = spi_xfer(this->spi_interface, RD_NXT_MSG);
    //cs high
}

bool MAX17841B::CheckReceiveBufferError() {
    //cs low
    spi_xfer(this->spi_interface, RD_RX_INT_FLAGS);
    uint8_t check = spi_xfer(this->spi_interface, RD_RX_INT_FLAGS);
    //cs high

    if (check != 0x00)
    {
        // Clear error
        //cs low
        spi_xfer(this->spi_interface, WR_RX_INT_FLAGS);
        spi_xfer(this->spi_interface, 0x00);
        //cs high
    }

    return check != 0x00;
}

uint16_t MAX17841B::ReadAddressedSlave(uint8_t dataRegister, uint8_t address, bool setupDone)
{
	uint8_t writeData[5] = {
        (uint8_t)(CMD_READ_DEVICE | (address << 3)),
        dataRegister,
        0x00 //Data-check byte (seed value = 0x00)
    };
    writeData[4] = 0x00;//CalculatePEC(data, 3);
    writeData[5] = 0x00; //Alive-counter byte (seed value = 0x00)
	
    LoadTransmitQueue(writeData, sizeof(writeData));
    //TransmitQueue();

    uint8_t readData[7]; //5 + (2 x n)
    ReadReceiveQueue(readData, sizeof(readData));
    
    bool error = false;
    
    error |= CheckReceiveBufferError();

    // Verify that the device register data is what was written during the WRITEDEVICE sequence
    for (uint8_t i = 0; i < 2; i++)
    {
        if (readData[i] != writeData[i]) {
            error |= true;
            break;
        }
    }

    if (setupDone)
    {
        //Check check-byte
        if (readData[4] != 0x00)
        {
            error |= true;
            //TODO: Clear STATUS register of addressed module
        }

        //Check PEC
        uint8_t checkPEC = 0x00;//CalculatePEC(readData, 5);
        if (readData[5] != checkPEC)
        {
            error |= true;
        }

        //Check alive-counter
        if (readData[6] != 0x01)
        {
            error |= true;
        }
    }

    //Try again if error
    static uint8_t readAttempts = 0;
    readAttempts += 1;
    if (error && (readAttempts < 3))
    {
        //delay(1);
        ReadAddressedSlave(dataRegister, address, setupDone);
    } else {
        readAttempts = 0;
    }

    return (readData[2] << 8) | readData[3];
}

void MAX17841B::WriteAddressedSlave(uint8_t dataRegister, uint16_t data, uint8_t address, bool setupDone)
{
	uint8_t writeData[6] = {
        (uint8_t)(CMD_WRITE_DEVICE | (address << 3)),
        dataRegister, 
        (uint8_t)(data & 0xFF),
        (uint8_t)((data >> 8) & 0xFF),
    };
    writeData[5] = 0x00;//CalculatePEC(data, 4);
    writeData[6] = 0x00; //Alive-counter byte (seed value = 0x00)
	
    LoadTransmitQueue(writeData, sizeof(writeData));
    //TransmitQueue();

    uint8_t readData[6];
    ReadReceiveQueue(readData, sizeof(readData));
    
    bool error = false;
    
    error |= CheckReceiveBufferError();

    // Verify that the device register data is what was written during the WRITEDEVICE sequence
    for (uint8_t i = 0; i < 5; i++)
    {
        if (readData[i] != writeData[i]) {
            error |= true;
            break;
        }
    }

    if (setupDone)
    {
        //Check alive-counter
        if (readData[5] != 0x01)
        {
            error |= true;
        }
    }

    //Try again if error
    static uint8_t writeAttempts = 0;
    writeAttempts += 1;
    if (error && (writeAttempts < 3))
    {
        //delay(1);
        WriteAddressedSlave(dataRegister, data, address, setupDone);
    } else {
        writeAttempts = 0;
    }
}
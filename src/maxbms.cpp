#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "maxbms.h"
#include "params.h"
#include "digio.h"
#include "hwdefs.h"
#include "max17841b.h"

MAX17841B max17841b(SPI2, &DigIo::max_cs);

void MaxBms::Init()
{
    uint8_t model = max17841b.ReadRegister(0x15);
    Param::SetInt(Param::test, model);

}

//void Ms100Task() 
//{
    // Read either all or cycle through cell voltages
    // Determine balance requirements
    // Either set all or cycle through balancing
//}

//void Ms2Task()
//{
    // Switch MUX
//}
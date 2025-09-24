#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/spi.h>
#include "selftest.h"
#include "maxbms.h"
#include "params.h"
#include "digio.h"
#include "hwdefs.h"
#include "max17841b.h"

MAX17841B max17841b(SPI2, &DigIo::max_cs);

SelfTest::TestFunction MaxBms::testFunctions[] = {
   MaxChipIdentityTest
};

void MaxBms::Init()
{

}

SelfTest::TestResult MaxBms::MaxChipIdentityTest() {
    uint8_t model = max17841b.ReadRegister(0x15);
    uint8_t version = max17841b.ReadRegister(0x17);
    Param::SetInt(Param::test, model);

    if (model != 0x84) {
        return SelfTest::TestFailed;
    }

    if (version != 0x12) {
        return SelfTest::TestFailed;
    }

    return SelfTest::TestSuccess;
};


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
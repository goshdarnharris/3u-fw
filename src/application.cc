
#include "application.hh"

#include "stm32g4xx_ll_ucpd.h"
#include "stm32g4xx_ll_pwr.h"

using namespace utl::literals;

extern "C" void SysTick_Handler(void) {
    HAL_IncTick();
    //this may need a call to   HAL_SYSTICK_IRQHandler();
}

application::application() :
    m_uart{USART2, 3000000u},
    m_application_output{m_uart},
    m_logger_setup{&m_application_output},
    m_power_switch{GPIOA, GPIO_PIN_1, utl::driver::pin::active_level::high, true, GPIO_NOPULL, GPIO_SPEED_FREQ_MEDIUM},
    m_adc{ADC1, 1000u, 3.3f},
    m_current_sense{m_adc ? &m_adc.value() : nullptr, ADC_CHANNEL_1}
{
    if(!m_uart) while(1);
}

void application::start(void)
{
    utl::log("application started");
    utl::log("uart is running at %d", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART2));
    // utl::log("\n");
    // m_uart.value().write("U",1);

    // 1. set up ucpd clocks
    // 2. configure sink mode LL_UCPD_SetSNKRole
    // 3. init ucpd LL_UCPD_Init
    // 4. enable ucpd LL_UCPD_Enable
    // 5. enable detection on CC1 & CC2 (LL_UCPD_TypeCDetectionCC2Enable, LL_UCPD_TypeCDetectionCC1Enable)
    // 6. poll type C state (LL_UCPD_GetTypeCVstateCC1, LL_UCPD_GetTypeCVstateCC2)

    //set sink mode
    //LL_UCPD_SetSNKRole()

    //for either:
    // 0b00 - vRa
    // 0b01 - vRd-USB
    // 0b10 - vRd-1.5
    // 0b11 - vRd-3.0
    //LL_UCPD_GetTypeCVstateCC2()
    //LL_UCPD_GetTypeCVstateCC1()
    __HAL_RCC_UCPD1_CLK_ENABLE();
    LL_UCPD_InitTypeDef config;
    LL_UCPD_StructInit(&config);
    LL_UCPD_Init(UCPD1, &config);
    LL_UCPD_Enable(UCPD1);
    LL_UCPD_SetSNKRole(UCPD1);
    LL_UCPD_SetccEnable(UCPD1, LL_UCPD_CCENABLE_CC1CC2);
    LL_UCPD_TypeCDetectionCC1Enable(UCPD1);
    LL_UCPD_TypeCDetectionCC2Enable(UCPD1);
    LL_PWR_DisableUSBDeadBattery();

    //thoughts on ws2812 interface:
    // PWM output with DMA
    // DMA sets the compare register
    // DMA transfer is 8 bytes long; 1 byte per bit in the input
    // at each transfer, I take the input data byte and run it through a lookup table
    //  to get the 8 bytes for that DMA transfer
    // lookup table needs to be ~2k. 256 indices, each holding 8 bytes.
    // this means that every 8 bits, I need to:
    //  grab a byte from the source buffer (1 dereference)
    //  perform a lookup (1 dereference)
    //  reconfigure the DMA source address (1 register write)
    //  re-start DMA (1 register write)

    // otherwise, I could just track a bit shift and set the compare register after every cycle.
    //  1 bit shift
    //  1 branch (or 1 multiply, 1 add)
    //  1 register write
    //  every 3 bytes, I need to load a new word in. so that's:
    //  increment a counter
    //  on counter overflow, reset it and load a new byte from the source buffer
    // this is easier to start with, probably.
    utl::log("PD: %d, %d", LL_UCPD_GetTypeCVstateCC1(UCPD1) >> UCPD_SR_TYPEC_VSTATE_CC1_Pos, LL_UCPD_GetTypeCVstateCC2(UCPD1) >> UCPD_SR_TYPEC_VSTATE_CC2_Pos);


    

    m_power_switch.set_state(true);
    if(!m_adc) utl::log("ADC initialization failed: %s", m_adc.error().message().data());
}

void application::loop(void)
{
    HAL_Delay(1000);
    if(m_adc) {
        auto calculate_current = [](const uint16_t conversion) {
            return 161u*static_cast<uint32_t>(conversion)/100u;
        };

        auto conv = m_current_sense.conversion();
        if(conv) {
            utl::log("%d?", conv.value());
            auto current = calculate_current(conv.value());
            utl::log("%dmA", current);
        } else {
            utl::log("conversion failed.");
        }
    }
}

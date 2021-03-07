// This program will measure debounce time between 2 GPIO pins
// First pin rise and fall will start the timer counter, the second pin interrupt will be used to get the counter value
// First pulse needs to be wider than 2us, 1us is the minimum measurable debounce time and it is measured as 820ns
// For debounce times of 2us or more expect +/- 1 clock cycle inaccuracy (20ns)
// Maximum value for rise or fall debounce time is 4 seconds.
volatile uint32_t debounce_rise;
volatile uint32_t debounce_fall;
volatile uint32_t eic_hold_count_rise;
volatile uint32_t eic_hold_count_fall;
volatile char     serial_command;

#define SerialUSB Serial // this is needed for trinket m0
#define PIN_1 7          // 7  is PB9 on XIAO, 2 is PA9 on trinket m0, both have pin #9 which is odd
#define PIN_2 8          // 8  is PA7 on XIAO

void setup()
{
  SerialUSB.begin(115200);                       // Send data back on the native port
  while(!SerialUSB);                             // Wait for the SerialUSB port to be ready
 
  REG_PM_APBCMASK |= PM_APBCMASK_EVSYS |         // Switch on the event system peripheral
                     PM_APBCMASK_TC4   |         // Switch on the TC4 peripheral
                     PM_APBCMASK_TC5;            // Switch on the TC5 peripheral

  REG_PM_APBAMASK |= PM_APBAMASK_EIC;            // Switch on the EIC external interrupt

  REG_GCLK_GENDIV = GCLK_GENDIV_DIV(0) |         // Divide the 48MHz system clock by 1 = 48MHz
                    GCLK_GENDIV_ID(1);           // Set division on Generic Clock Generator (GCLK) 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC |          // Set the duty cycle to 50/50 HIGH/LOW
                     GCLK_GENCTRL_GENEN |        // Enable GCLK
                     GCLK_GENCTRL_SRC_DFLL48M |  // Set the clock source to 48MHz
                     GCLK_GENCTRL_ID(1);         // Set clock source on GCLK 1
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN      |   // Enable the generic clock
                     GCLK_CLKCTRL_GEN_GCLK1  |   // on GCLK1
                     GCLK_CLKCTRL_ID_EIC;        // Feed the GCLK1 also to EIC
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN      |   // Enable the generic clock
                     GCLK_CLKCTRL_GEN_GCLK1  |   // on GCLK1
                     GCLK_CLKCTRL_ID_TC4_TC5;    // Feed the GCLK1 also to TC4
  while (GCLK->STATUS.bit.SYNCBUSY);             // Wait for synchronization

  // Enable the port multiplexer on pin number "PIN_1"
  PORT->Group[g_APinDescription[PIN_1].ulPort].PINCFG[g_APinDescription[PIN_1].ulPin].bit.PULLEN = 1; // out is default low so pull-down
  PORT->Group[g_APinDescription[PIN_1].ulPort].PINCFG[g_APinDescription[PIN_1].ulPin].bit.INEN   = 1;
  PORT->Group[g_APinDescription[PIN_1].ulPort].PINCFG[g_APinDescription[PIN_1].ulPin].bit.PMUXEN = 1;
  // Set-up the pin as an EIC (interrupt) peripheral on an odd pin. "0" means odd, "A" function is EIC
  PORT->Group[g_APinDescription[PIN_1].ulPort].PMUX[g_APinDescription[PIN_1].ulPin >> 1].reg |= PORT_PMUX_PMUXO_A;

  // Enable the port multiplexer on pin number "PIN_2"
  PORT->Group[g_APinDescription[PIN_2].ulPort].PINCFG[g_APinDescription[PIN_2].ulPin].bit.PULLEN = 1; // out is default low so pull-down
  PORT->Group[g_APinDescription[PIN_2].ulPort].PINCFG[g_APinDescription[PIN_2].ulPin].bit.INEN   = 1;
  PORT->Group[g_APinDescription[PIN_2].ulPort].PINCFG[g_APinDescription[PIN_2].ulPin].bit.PMUXEN = 1;
  // Set-up the pin as an EIC (interrupt) peripheral on an odd pin. "0" means odd, "A" function is EIC
  PORT->Group[g_APinDescription[PIN_2].ulPort].PMUX[g_APinDescription[PIN_2].ulPin >> 1].reg |= PORT_PMUX_PMUXO_A;

  EIC->EVCTRL.reg     |= EIC_EVCTRL_EXTINTEO9;                           // Enable EXTINT9 (PB09)
  EIC->CONFIG[1].reg  |= EIC_CONFIG_SENSE1_BOTH;                         // Set event detecting both edges (config 1, #1 is 9)
  EIC->INTENSET.reg   |= EIC_INTENSET_EXTINT9;                           // Set the interrupt flag on channel 9 (PB09)
  while (EIC->STATUS.bit.SYNCBUSY);                                      // Wait for synchronization

  EIC->EVCTRL.reg     |= EIC_EVCTRL_EXTINTEO7;                           // Enable EXTINT7 (PA07)
  EIC->CONFIG[0].reg  |= EIC_CONFIG_SENSE7_BOTH;                         // Set event detecting both edges (config 0, #7 is 7)
  EIC->INTENSET.reg   |= EIC_INTENSET_EXTINT7;                           // Set the interrupt on channel 7 (PA07)
  while (EIC->STATUS.bit.SYNCBUSY);                                      // Wait for synchronization

  EIC->CTRL.reg        = EIC_CTRL_ENABLE;                                // enable EIC
  while (EIC->STATUS.bit.SYNCBUSY);                                      // Wait for synchronization

  NVIC_EnableIRQ( EIC_IRQn );                    // Enable EIC interrupts

  REG_EVSYS_CHANNEL = EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT |              // No event edge detection, we already have it on the EIC
                      EVSYS_CHANNEL_PATH_ASYNCHRONOUS    |              // Set event path as asynchronous
                      EVSYS_CHANNEL_EVGEN(EVSYS_ID_GEN_EIC_EXTINT_9) |  // Set event generator (sender) as external interrupt 9 (PB09)
                      EVSYS_CHANNEL_CHANNEL(0);                         // Attach the generator (sender) to channel 0

  REG_EVSYS_USER = EVSYS_USER_CHANNEL(1) |                              // Attach the event user (receiver) to channel 0 (n + 1)
                   EVSYS_USER_USER(EVSYS_ID_USER_TC4_EVU);              // Set the event user (receiver) as timer TC4

  REG_TC4_EVCTRL  |= TC_EVCTRL_TCEI |            // Enable the TC event input
                     TC_EVCTRL_EVACT_START;      // Set up the timer for capture: start on event
  
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (write) synchronization
 
  REG_TC4_CTRLA |= TC_CTRLA_PRESCALER_DIV1 |     // Set prescaler to 1, 48MHz/1 = 48MHz
                   TC_CTRLA_MODE_COUNT32   |     // Set the TC4 timer to 32-bit mode in conjuction with timer TC5
                   TC_CTRLA_ENABLE;              // Enable TC4
  while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for synchronization

  eic_hold_count_rise = 0;
  eic_hold_count_fall = 0;
}


void loop()
{
  if (SerialUSB.available())
  {
    noInterrupts();
    serial_command = SerialUSB.read();
    if (serial_command == 'c')
    {
      eic_hold_count_rise = 0;
      eic_hold_count_fall = 0;
    }
    else
    {
      if (eic_hold_count_rise > 53) // I need to reduce the rise count by 53 clock cycles
      {
        debounce_rise = (eic_hold_count_rise - 53) * (1000.0 / 48.0);
      }
      else
      {
        debounce_rise = 0;
      }
      if (eic_hold_count_fall > 54) // I need to reduce the fall count by 54 clock cycles
      {
        debounce_fall = (eic_hold_count_fall - 54) * (1000.0 / 48.0);
      }
      else
      {
        debounce_fall = 0;
      }
      SerialUSB.print("debounce_rise=");
      SerialUSB.println(debounce_rise);
      SerialUSB.print("debounce_fall=");
      SerialUSB.println(debounce_fall);
      SerialUSB.print("--> input c to clear counts <--\n");
      REG_TC4_CTRLBSET = TC_CTRLBSET_CMD_STOP;
      REG_TC4_COUNT32_COUNT = 0;
      while (TC4->COUNT32.STATUS.bit.SYNCBUSY);      // Wait for (write) synchronization
    }
    interrupts();
  }
  delay(10);
}

void EIC_Handler()  // Interrupt Service Routine (ISR) for external
{
  if (EIC->INTFLAG.bit.EXTINT7)
  {
    if (eic_hold_count_rise == 0x00000000)
    {
      eic_hold_count_rise = REG_TC4_COUNT32_COUNT;
      REG_TC4_CTRLBSET = TC_CTRLBSET_CMD_STOP;
      REG_TC4_COUNT32_COUNT = 0;
    }
    else
    {
      eic_hold_count_fall = REG_TC4_COUNT32_COUNT;
      REG_TC4_CTRLBSET = TC_CTRLBSET_CMD_STOP;
      REG_TC4_COUNT32_COUNT = 0;
    }
  }
  EIC->INTFLAG.reg = 0xFFFFFFFF;
}

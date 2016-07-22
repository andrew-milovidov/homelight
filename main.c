#include "iostm8s103f3.h"
#include "stdint.h"
/*
pd4 - выход 2
pd3 - выход 1
pd2 - вход с потенциометра 1
pc7 - вход кнопка, Pull-up
pc6 - вход с датчика 1
pc5 - вход с датчика 2
pc4 - вход с потенциометра 2
pc3 - вход кнопка 2, Pull-up
*/

#define flash_version 25

#pragma location=0x4000
__no_init uint8_t flashReady;
#pragma location=0x4001
__no_init uint8_t minutesCount;
#pragma location=0x4002
__no_init uint16_t startUptime;
#pragma location=0x4004
__no_init uint16_t fallUptime;


uint8_t lastIR1 = 0;
uint8_t lastIR2 = 0;
uint8_t lastBut1 = 0;
uint8_t lastBut2 = 0;
uint8_t maxPower = 20;

uint16_t counter = 0;

void delay100ms()
{
  uint16_t i;
  for(i = 0; i < 5000; i++)
  {
    asm("nop");
  }
}

void outChannel1(uint16_t pwr)
{
  TIM2_CCR1L = (pwr & 0xFF);
}

void outChannel2(uint16_t pwr)
{
  TIM2_CCR2L = (pwr & 0xFF);
}

void startADC()
{
  ADC_CR1_ADON = 1;
}

int main( void )
{
  //setup mains
  CLK_CKDIVR_bit.CPUDIV = 4; //div by 16 (1 MHz)
  CLK_CKDIVR_bit.HSIDIV = 2; //div by 4 (250 kHz)
  
  //setup inputs
  PC_DDR = 0;//all input
  PC_CR1 = 0x11; //0b00010001;//pullup on pc3, pc7
  
  //setup outputs
  PB_DDR_DDR5 = 1;
  PB_CR1_C15 = 1;//push-pull
  PB_ODR_ODR5 = 0;
  
  PD_DDR = 0xFF;//all output
  PD_DDR_DDR2 = 0;
  PD_CR1_C13 = 1;//push-pull
  PD_CR1_C14 = 1;//push-pull
  PD_ODR = 0;//set output to 0 
  
  //setup timers
  TIM2_PSCR = 0;
  TIM2_ARRH = 0x00;
  TIM2_ARRL = 0xFF; //~1 kHz PWM width
  TIM2_CCR1H = 0;
  TIM2_CCR2H = 0;
  TIM2_CR1_ARPE = 0;//autoreload enamle
  TIM2_CCER1_CC1P = 0;// Active high
  TIM2_CCER1_CC2P = 0;
  TIM2_CCER1_CC1E = 1;// enable compare mode
  TIM2_CCER1_CC2E = 1;
  TIM2_CCMR1_OC1M = 6;
  TIM2_CCMR2_OC2M = 6;
  outChannel1(0);
  outChannel2(0);
  TIM2_CR1_CEN = 1;
  
  TIM2_CCER1_CC1P = 0;
  //setup adc
  CLK_PCKENR2 |= 4;//PCKEN23 - enable ADC1
  ADC_CSR_CH = 3;
  
  
  
  //setup flash
  FLASH_DUKR = 0xAE;
  FLASH_DUKR = 0x56; 
  while (!(FLASH_IAPSR & (1<<3))) //wait for flash ready
    ;
  if (flashReady != flash_version) //check version of stored settings. every shema change flashReady must be updated
  {
    minutesCount = 1;
    startUptime = 40;
    fallUptime = 40;
    flashReady = flash_version;
  }
  
  //start 
  //asm("rim");
  uint16_t time1 = 0;
  uint16_t time2 = 0;
  uint8_t run1 = 0;
  uint8_t run2 = 0;
  uint8_t fallCounter1 = 0;
  uint8_t fallCounter2 = 0;
  while (1)
  {
    uint8_t ir1 = PC_IDR_IDR6;
    uint8_t ir2 = PC_IDR_IDR5;
    if (ir1 != lastIR1)
    {
      if (ir1)
      {
        run1 = 1;
        fallCounter1 = 0;
        if (time1 > startUptime)
          time1 = startUptime;
      }
    }
    if (ir2 != lastIR2)
    {
      if (ir2 && time2 == 0)
        run2 = 1;
    }
    
    lastIR1 = ir1;
    lastIR2 = ir2;
    
    
    if (counter > 200)
    {
      counter = 0;
      PB_ODR_ODR5 = !PB_ODR_ODR5;
      if (run1 != 0)
      {
        time1++;
        if (time1 < startUptime)
        {
          outChannel1(time1 * maxPower / startUptime);
        }
        else if (time1 > 10 * 60 * minutesCount)
        {
          run1 = 0;
          time1 = 0;
          fallCounter1 = fallUptime;
        }
      } 
      else if (fallCounter1 > 0)
      {
        outChannel1(fallCounter1 * maxPower / fallUptime);
        fallCounter1--;
        if (fallCounter1 == 0)
          outChannel1(0);
      }
      if (run2 != 0)
      {
        time2++;
        if (time2 < startUptime)
          outChannel2(time2 * maxPower / startUptime);
        else if (time2 > 10 * 60 * minutesCount)
        {
          run2 = 0;
          time2 = 0;
          fallCounter2 = fallUptime;
        }
      } 
      else if (fallCounter2 > 0)
      {
        outChannel2(fallCounter2 * maxPower / fallUptime);
        fallCounter2--;
        if (fallCounter2 == 0)
          outChannel2(0);
      }
    }
    else
    {
      counter++;
    }
  }
  
  return 0;
}
//
//#pragma vector = TIM1_OVR_UIF_vector
//__interrupt void TIM1_OVR_UIF(void)
//{
//  TIM1_SR1_bit.UIF = 0;
//  //timCounter++;
//}
//
//#pragma vector = 7
//__interrupt void EXTI_PORTC_IRQHandler(void)
//{  
//  //mirfIrq = 1; 
//}

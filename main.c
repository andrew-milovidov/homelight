#include "iostm8s103f3.h"
#include "stdint.h"

/*
pd4 - output 2
pd3 - output 1
pd2 - reserved, not used
pc7 - input button 1, Pull-up, menu
pc6 - input sensor 1
pc5 - input sensor 2
pc4 - input button 2, Pull-up, plus
pc3 - input button 3, Pull-up, minus
*/

#define flash_version 29
#define MAX_POWER 20
#define MAX_MINUTES 10
#define SETTINGS_COUNTER 300
#define MAX_RISE_FALLTIME 100
#define WAIT_COUNTER 200
#define ENABLE_MENU 0
#define ENABLE_BUTTONS 1

#pragma location=0x4000
__no_init uint8_t flashReady;
#pragma location=0x4001
__no_init uint8_t minutesCount;
#pragma location=0x4002
__no_init uint16_t startUptime;
#pragma location=0x4004
__no_init uint16_t fallUptime;
#pragma location=0x4006
__no_init uint8_t maxPower;

uint8_t lastIR1 = 0;
uint8_t lastIR2 = 0;
uint8_t lastBut1 = 0;
uint8_t lastBut2 = 0;
uint8_t lastB1 = 1;
uint8_t lastB2 = 1;
uint8_t lastB3 = 1;
uint16_t counter = 0;
uint8_t currentItem = 0;
uint8_t menuArray[256];
uint8_t menuArrayLen = 0;
uint8_t menuArrayCurrent = 0;
uint8_t menuArrayCounter = 0;

void outChannel1(uint16_t pwr)
{
  TIM2_CCR1L = (pwr & 0xFF);
}

void outChannel2(uint16_t pwr)
{
  TIM2_CCR2L = (pwr & 0xFF);
}

void fillMenu(uint8_t menuNum, uint8_t value)
{
  uint8_t i = 0;
  uint8_t k = 0;
  menuArrayLen = 2 + value * 2;
  for (i = 0; i < menuNum; i++)
  {
    menuArray[k] = 6;
    k++;
    menuArray[k] = 2;
    k++;
  }
  for (i = 0; i < value; i++)
  {
    menuArray[k] = 1;
    k++;
    menuArray[k] = 3;
    k++;
  }
  menuArrayCurrent = 0;
  menuArrayCounter = 0;
}

int main( void )
{
  //setup mains
  CLK_CKDIVR_bit.CPUDIV = 4; //div by 16 (1 MHz)
  CLK_CKDIVR_bit.HSIDIV = 2; //div by 4 (250 kHz)
  
  //setup inputs
  PC_DDR = 0;//all input
  PC_CR1 = 0x00; //0b01001100;//pullup on pc3, pc4, pc7
  //PC_CR1_C17 = 1;
  //PC_CR1_C14 = 1;
  //PC_CR1_C13 = 1;
  
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
  
  //setup flash
  FLASH_DUKR = 0xAE;
  FLASH_DUKR = 0x56; 
  while (!(FLASH_IAPSR & (1<<3))) //wait for flash ready
    ;
  if (flashReady != flash_version) //check version of stored settings. every shema change flashReady must be updated
  {
    minutesCount = 3;
    startUptime = 30;
    fallUptime = 30;
    maxPower = 4;
    flashReady = flash_version;
  } 
  else 
  {
    if (maxPower > MAX_POWER)
      maxPower = MAX_POWER;
    if (minutesCount > MAX_MINUTES)
      minutesCount = MAX_MINUTES;
  }
  
  //start 
  uint16_t time1 = 0;
  uint16_t time2 = 0;
  uint8_t run1 = 0;
  uint8_t run2 = 0;
  uint8_t fallCounter1 = 0;
  uint8_t fallCounter2 = 0;
  uint16_t settingsCounter = 0;
  uint8_t b1Pressed = 0;
  uint8_t b2Pressed = 0;
  uint8_t b3Pressed = 0;
  uint16_t pwm1 = 0;
  uint16_t pwm2 = 0;
  uint8_t temp = 0;
  uint8_t menuMode = 0;
  if (PC_IDR_IDR7 == 0)//failsafe on startup
  {
    maxPower = 0;
    lastB1 = 0;//no menu enter
  }
  while (1)
  {
    //sensors detector
    temp = PC_IDR_IDR6;
    if (temp != lastIR1)
    {
      if (temp)
      {
        run1 = 1;
        fallCounter1 = 0;
        if (time1 > startUptime)
          time1 = startUptime;
      }
    }
    lastIR1 = temp;
    temp = PC_IDR_IDR5;
    if (temp != lastIR2)
    {
      if (temp && time2 == 0)
        run2 = 1;
    }
    lastIR2 = temp;
    if (ENABLE_BUTTONS)
    {
      //button press detector
      temp = PC_IDR_IDR7;
      if (temp != lastB1 && temp == 0)
        b1Pressed = 1;
      lastB1 = temp;
      temp = PC_IDR_IDR4;
      if (temp != lastB2 && temp == 0)
        b2Pressed = 1;
      lastB2 = temp;
      temp = PC_IDR_IDR3;
      if (temp != lastB3 && temp == 0)
        b3Pressed = 1;
      lastB3 = temp;
    }
    //logic
    if (counter < WAIT_COUNTER)
    {
      counter++;
      continue;
    }
    counter = 0;
    PB_ODR_ODR5 = !PB_ODR_ODR5;
    if (run1 != 0)
    {
      time1++;
      if (time1 < startUptime)
      {
        pwm1 = time1 * maxPower / startUptime;
      }
      else if (time1 > 10 * 60 * minutesCount)
      {
        run1 = 0;
        time1 = 0;
        fallCounter1 = fallUptime;
      }
      else
      {
        pwm1 = maxPower;
      }
    } 
    else if (fallCounter1 > 0)
    {
      pwm1 = fallCounter1 * maxPower / fallUptime;
      fallCounter1--;
      if (fallCounter1 == 0)
        pwm1 = 0;
    }
    else
    {
      pwm1 = 0;
    }
    if (run2 != 0)
    {
      time2++;
      if (time2 < startUptime)
        pwm2 = time2 * maxPower / startUptime;
      else if (time2 > 10 * 60 * minutesCount)
      {
        run2 = 0;
        time2 = 0;
        fallCounter2 = fallUptime;
      }
      else
      {
        pwm2 = maxPower;
      }
    } 
    else if (fallCounter2 > 0)
    {
      pwm2 = fallCounter2 * maxPower / fallUptime;
      fallCounter2--;
      if (fallCounter2 == 0)
        pwm2 = 0;
    }
    else
    {
      pwm2 = 0;
    }
    if (b1Pressed)
    {
      if (settingsCounter == 0)
        menuMode = 0;
      settingsCounter = SETTINGS_COUNTER;
    }
    if (ENABLE_MENU == 0 || settingsCounter == 0) {
      if (b2Pressed == 1)
      {
        if (maxPower < MAX_POWER)
          maxPower++;
        b2Pressed = 0;
      }
      if (b3Pressed == 1)
      {
        if (maxPower > 0)
          maxPower--;
        b3Pressed = 0;
      }
    }
    else
    {//in system menu, no sensor effect;
      settingsCounter--;
      if (settingsCounter == 0)
      {
        menuMode = 0;
      }
      if (b1Pressed)
      {
        if (menuMode == 0 || menuMode > 2)
          menuMode = 1;
        else 
          menuMode++;
      }
      else if (b2Pressed)
      {
        if (menuMode == 1)
        {
          if (minutesCount < MAX_MINUTES)
            minutesCount++;
        } 
        else if (menuMode == 2)
        {
          if (startUptime < MAX_RISE_FALLTIME)
            startUptime++;
        } 
        else if (menuMode == 3)
        {
          if (fallUptime < MAX_RISE_FALLTIME)
            fallUptime++;
        }
      }
      else if (b3Pressed)
      {
        if (menuMode == 1)
        {
          if (minutesCount > 1)
            minutesCount--;
        } 
        else if (menuMode == 2)
        {
          if (startUptime > 1)
            startUptime--;
        } 
        else if (menuMode == 3)
        {
          if (fallUptime > 1)
            fallUptime--;
        }
      }
      if (b1Pressed || b2Pressed || b3Pressed)
      {
        fillMenu(menuMode, menuMode == 1 ? minutesCount : (menuMode == 2 ? startUptime : fallUptime));
        b1Pressed = 0;
        b2Pressed = 0;
        b3Pressed = 0;
      } 
      else 
      {
        if (menuArrayCurrent < menuArrayLen)
        {
          uint8_t v = menuArray[menuArrayCurrent];
          pwm1 = menuArrayCurrent % 2 == 0 ? maxPower : 0;
          if (menuArrayCounter < v)
          {
            menuArrayCounter++;
          } 
          else 
          {
            menuArrayCurrent++;
            menuArrayCounter = 0;
          }
        } 
        else
        {
          menuArrayCurrent = 0;
          pwm1 = 0;
        }
      }
      pwm2 = 0;
    }
    outChannel1(pwm1);
    outChannel2(pwm2);
  }
}

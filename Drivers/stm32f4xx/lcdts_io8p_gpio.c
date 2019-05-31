/*
 * 8 bites p�rhuzamos LCD/TOUCH GPIO driver STM32F1-re
 * 5 vez�rl�l�b (CS, RS, WR, RD, RST) + 8 adatl�b + h�tt�rvil�git�s vez�rl�s

 * Figyelem: mivel azonos l�bakon van az Lcd �s a Touchscreen,
 * ez�rt ezek ki kell z�rni az Lcd �s a Touchscreen egyidej� haszn�lat�t!
 * T�bbsz�las/megszakit�sos k�rnyezetben igy gondoskodni kell az �sszeakad�sok megel�z�s�r�l!
 */

/* K�szit�: Roberto Benjami
   verzio:  2019.05

   Megj:
   Minden f�ggv�ny az adatl�bak ir�ny�t WRITE �zemmodban hagyja, igy nem kell minden ir�si
   m�veletkor �llitgatni
*/

/* CS l�b vez�rl�si strat�gia
   - 0: CS l�b minden ir�s/olvas�s m�velet sor�n �llitva van (igy a touchscreen olvas�sakor nem sz�ks�ges lekapcsolni
   - 1: CS l�b folyamatosan 0-ba van �llitva (a touchscreen olvas�sakor ez�rt le kell kapcsolni)
*/
#define  LCD_CS_MODE          0

// ADC sample time (0: 1.5cycles, 1: 7.5c, 2:13.5c, 3:28.5c, 4:41.5c, 5:55.5c, 6:71.5c, 7:239.5cycles)
#define  TS_SAMPLETIME        3

// A kijelz�n bel�l a k�vetkez� l�bak vannak p�rhuzamositva:
#define  TS_XP                LCD_D6
#define  TS_XM                LCD_RS
#define  TS_YP                LCD_WR
#define  TS_YM                LCD_D7

#include "main.h"
#include "lcdts_io8p_gpio.h"

/* Link function for LCD peripheral */
void     LCD_Delay (uint32_t delay);
void     LCD_IO_Init(void);
void     LCD_IO_Bl_OnOff(uint8_t Bl);

void     LCD_IO_WriteCmd8(uint8_t Cmd);
void     LCD_IO_WriteCmd16(uint16_t Cmd);
void     LCD_IO_WriteData8(uint8_t Data);
void     LCD_IO_WriteData16(uint16_t Data);

void     LCD_IO_WriteCmd8DataFill16(uint8_t Cmd, uint16_t Data, uint32_t Size);
void     LCD_IO_WriteCmd8MultipleData8(uint8_t Cmd, uint8_t *pData, uint32_t Size);
void     LCD_IO_WriteCmd8MultipleData16(uint8_t Cmd, uint16_t *pData, uint32_t Size);
void     LCD_IO_WriteCmd16DataFill16(uint16_t Cmd, uint16_t Data, uint32_t Size);
void     LCD_IO_WriteCmd16MultipleData8(uint16_t Cmd, uint8_t *pData, uint32_t Size);
void     LCD_IO_WriteCmd16MultipleData16(uint16_t Cmd, uint16_t *pData, uint32_t Size);

void     LCD_IO_ReadCmd8MultipleData8(uint8_t Cmd, uint8_t *pData, uint32_t Size, uint32_t DummySize);
void     LCD_IO_ReadCmd8MultipleData16(uint8_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize);
void     LCD_IO_ReadCmd8MultipleData24to16(uint8_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize);
void     LCD_IO_ReadCmd16MultipleData8(uint16_t Cmd, uint8_t *pData, uint32_t Size, uint32_t DummySize);
void     LCD_IO_ReadCmd16MultipleData16(uint16_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize);
void     LCD_IO_ReadCmd16MultipleData24to16(uint16_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize);

/* Link function for Touchscreen */
uint8_t  TS_IO_DetectToch(void);
uint16_t TS_IO_GetX(void);
uint16_t TS_IO_GetY(void);
uint16_t TS_IO_GetZ1(void);
uint16_t TS_IO_GetZ2(void);

// portl�b m�dok (PP: push-pull, OD: open drain, FF: input floating)
#define MODE_DIGITAL_INPUT    0x0
#define MODE_OUT              0x1
#define MODE_ALTER            0x2
#define MODE_ANALOG_INPUT     0x3

#define MODE_SPD_LOW          0x0
#define MODE_SPD_MEDIUM       0x1
#define MODE_SPD_HIGH         0x2
#define MODE_SPD_VHIGH        0x3

#define MODE_PU_NONE          0x0
#define MODE_PU_UP            0x1
#define MODE_PU_DOWN          0x2

#define BITBAND_ACCESS(variable, bitnumber) *(volatile uint32_t*)(((uint32_t)&variable & 0xF0000000) + 0x2000000 + (((uint32_t)&variable & 0x000FFFFF) << 5) + (bitnumber << 2))

#define GPIOX_PORT_(a, b)     GPIO ## a
#define GPIOX_PORT(a)         GPIOX_PORT_(a)

#define GPIOX_PIN_(a, b)      b
#define GPIOX_PIN(x)          GPIOX_PIN_(x)

#define GPIOX_MODER_(a,b,c)   GPIO ## b->MODER = (GPIO ## b->MODER & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_MODER(a, b)     GPIOX_MODER_(a, b)

#define GPIOX_OSPEEDR_(a,b,c) GPIO ## b->OSPEEDR = (GPIO ## b->OSPEEDR & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_OSPEEDR(a, b)   GPIOX_OSPEEDR_(a, b)

#define GPIOX_PUPDR_(a,b,c)   GPIO ## b->PUPDR = (GPIO ## b->PUPDR & ~(3 << (2 * c))) | (a << (2 * c));
#define GPIOX_PUPDR(a, b)     GPIOX_PUPDR_(a, b)

#define GPIOX_ODR_(a, b)      BITBAND_ACCESS(GPIO ## a ->ODR, b)
#define GPIOX_ODR(a)          GPIOX_ODR_(a)

#define GPIOX_IDR_(a, b)      BITBAND_ACCESS(GPIO ## a ->IDR, b)
#define GPIOX_IDR(a)          GPIOX_IDR_(a)

#define GPIOX_LINE_(a, b)     EXTI_Line ## b
#define GPIOX_LINE(a)         GPIOX_LINE_(a)

#define GPIOX_PORTSRC_(a, b)  GPIO_PortSourceGPIO ## a
#define GPIOX_PORTSRC(a)      GPIOX_PORTSRC_(a)

#define GPIOX_PINSRC_(a, b)   GPIO_PinSource ## b
#define GPIOX_PINSRC(a)       GPIOX_PINSRC_(a)

// GPIO Ports Clock Enable
#define GPIOX_CLOCK_(a, b)    RCC_AHB1ENR_GPIO ## a ## EN
#define GPIOX_CLOCK(a)        GPIOX_CLOCK_(a)

#define GPIOX_PORTTONUM_A     1
#define GPIOX_PORTTONUM_B     2
#define GPIOX_PORTTONUM_C     3
#define GPIOX_PORTTONUM_D     4
#define GPIOX_PORTTONUM_E     5
#define GPIOX_PORTTONUM_F     6
#define GPIOX_PORTTONUM_G     7
#define GPIOX_PORTTONUM_H     8
#define GPIOX_PORTTONUM_J     9
#define GPIOX_PORTTONUM_K     10
#define GPIOX_PORTTONUM_L     11
#define GPIOX_PORTTONUM_M     12
#define GPIOX_PORTNUM_(p, m)  GPIOX_PORTTONUM_ ## p
#define GPIOX_PORTNAME_(p, m) p
#define GPIOX_PORTNUM(x)      GPIOX_PORTNUM_(x)
#define GPIOX_PORTNAME(x)     GPIOX_PORTNAME_(x)

//-----------------------------------------------------------------------------
// Parancs/adat l�b �zemmod
#define LCD_RS_CMD            GPIOX_ODR(LCD_RS) = 0
#define LCD_RS_DATA           GPIOX_ODR(LCD_RS) = 1

// Reset l�b aktiv/passziv
#define LCD_RST_ON            GPIOX_ODR(LCD_RST) = 0
#define LCD_RST_OFF           GPIOX_ODR(LCD_RST) = 1

// Chip select l�b
#if  LCD_CS_MODE ==  0
#define LCD_CS_ON             GPIOX_ODR(LCD_CS) = 0
#define LCD_CS_OFF            GPIOX_ODR(LCD_CS) = 1
#define LCD_TS_ON
#define LCD_TS_OFF
#endif

#if  LCD_CS_MODE ==  1
#define LCD_CS_ON
#define LCD_CS_OFF
#define LCD_TS_ON             GPIOX_ODR(LCD_CS) = 1
#define LCD_TS_OFF            GPIOX_ODR(LCD_CS) = 0
#endif

//-----------------------------------------------------------------------------
// Ha a 8 adatl�b egy porton bel�l emelked� sorrendben van -> automatikusan optimaliz�l
#if ((GPIOX_PORTNUM(LCD_D0) == GPIOX_PORTNUM(LCD_D1))\
  && (GPIOX_PORTNUM(LCD_D1) == GPIOX_PORTNUM(LCD_D2))\
  && (GPIOX_PORTNUM(LCD_D2) == GPIOX_PORTNUM(LCD_D3))\
  && (GPIOX_PORTNUM(LCD_D3) == GPIOX_PORTNUM(LCD_D4))\
  && (GPIOX_PORTNUM(LCD_D4) == GPIOX_PORTNUM(LCD_D5))\
  && (GPIOX_PORTNUM(LCD_D5) == GPIOX_PORTNUM(LCD_D6))\
  && (GPIOX_PORTNUM(LCD_D6) == GPIOX_PORTNUM(LCD_D7)))
#if ((GPIOX_PIN(LCD_D0) + 1 == GPIOX_PIN(LCD_D1))\
  && (GPIOX_PIN(LCD_D1) + 1 == GPIOX_PIN(LCD_D2))\
  && (GPIOX_PIN(LCD_D2) + 1 == GPIOX_PIN(LCD_D3))\
  && (GPIOX_PIN(LCD_D3) + 1 == GPIOX_PIN(LCD_D4))\
  && (GPIOX_PIN(LCD_D4) + 1 == GPIOX_PIN(LCD_D5))\
  && (GPIOX_PIN(LCD_D5) + 1 == GPIOX_PIN(LCD_D6))\
  && (GPIOX_PIN(LCD_D6) + 1 == GPIOX_PIN(LCD_D7)))
// LCD_D0..LCD_D7 l�bak azonos porton �s n�vekv� sorrendben vannak vannak
#define LCD_AUTOOPT
#endif // D0..D7 portl�b folytonoss�g ?
#endif // D0..D7 port azonoss�g ?

//-----------------------------------------------------------------------------
// adat l�bak kimenetre �llit�sa
#ifndef LCD_DATA_DIRWRITE
#ifdef  LCD_AUTOOPT
#define LCD_DATA_DIRWRITE  GPIOX_PORT(LCD_D0)->MODER = (GPIOX_PORT(LCD_D0)->MODER & ~(0xFFFF << (2 * GPIOX_PIN(LCD_D0)))) | (0x5555 << (2 * GPIOX_PIN(LCD_D0)));
#else   // #ifdef  LCD_AUTOOPT
#define LCD_DATA_DIRWRITE { \
  GPIOX_MODER(MODE_OUT, LCD_D0); GPIOX_MODER(MODE_OUT, LCD_D1);\
  GPIOX_MODER(MODE_OUT, LCD_D2); GPIOX_MODER(MODE_OUT, LCD_D3);\
  GPIOX_MODER(MODE_OUT, LCD_D4); GPIOX_MODER(MODE_OUT, LCD_D5);\
  GPIOX_MODER(MODE_OUT, LCD_D6); GPIOX_MODER(MODE_OUT, LCD_D7);}
#endif  // #else  LCD_AUTOOPT
#endif  // #ifndef LCD_DATA_DIROUT

//-----------------------------------------------------------------------------
// adat l�bak bemenetre �llit�sa
#ifndef LCD_DATA_DIRREAD
#ifdef  LCD_AUTOOPT
#define LCD_DATA_DIRREAD  GPIOX_PORT(LCD_D0)->MODER = (GPIOX_PORT(LCD_D0)->MODER & ~(0xFFFF << (2 * GPIOX_PIN(LCD_D0)))) | (0x0000 << (2 * GPIOX_PIN(LCD_D0)));
#else   // #ifdef  LCD_AUTOOPT
#define LCD_DATA_DIRREAD { \
  GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D0); GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D1);\
  GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D2); GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D3);\
  GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D4); GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D5);\
  GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D6); GPIOX_MODER(MODE_DIGITAL_INPUT, LCD_D7);}
#endif
#endif

//-----------------------------------------------------------------------------
// adat l�bakra 8 bites adat kiir�sa
#ifndef LCD_DATA_WRITE
#ifdef  LCD_AUTOOPT
#define LCD_DATA_WRITE(dt) { \
  GPIOX_PORT(LCD_D0)->BSRR = (dt << GPIOX_PIN(LCD_D0)) | (0xFF << (GPIOX_PIN(LCD_D0) + 16));
  GPIOX_ODR(LCD_WR) = 0; LCD_IO_Delay(LCD_IO_RW_DELAY); GPIOX_ODR(LCD_WR) = 1;}
#else   // #ifdef  LCD_AUTOOPT
uint8_t data;
#define LCD_DATA_WRITE(dt) {;                  \
  data = dt;                                   \
  GPIOX_ODR(LCD_D0) = BITBAND_ACCESS(data, 0); \
  GPIOX_ODR(LCD_D1) = BITBAND_ACCESS(data, 1); \
  GPIOX_ODR(LCD_D2) = BITBAND_ACCESS(data, 2); \
  GPIOX_ODR(LCD_D3) = BITBAND_ACCESS(data, 3); \
  GPIOX_ODR(LCD_D4) = BITBAND_ACCESS(data, 4); \
  GPIOX_ODR(LCD_D5) = BITBAND_ACCESS(data, 5); \
  GPIOX_ODR(LCD_D6) = BITBAND_ACCESS(data, 6); \
  GPIOX_ODR(LCD_D7) = BITBAND_ACCESS(data, 7); \
  GPIOX_ODR(LCD_WR) = 0; LCD_IO_Delay(LCD_IO_RW_DELAY); GPIOX_ODR(LCD_WR) = 1;}
#endif
#endif

//-----------------------------------------------------------------------------
// adat l�bakrol 8 bites adat beolvas�sa
#ifndef LCD_DATA_READ
#ifdef  LCD_AUTOOPT
#define LCD_DATA_READ(dt) {                  \
  GPIOX_ODR(LCD_RD) = 0;                     \
  LCD_IO_Delay(LCD_IO_RW_DELAY);             \
  dt = GPIOX_PORT(LCD_D0)->IDR >> GPIOX_PIN(LCD_D0); \
  GPIOX_ODR(LCD_RD) = 1;                     }
#else   // #ifdef  LCD_AUTOOPT
#define LCD_DATA_READ(dt) {                  \
  GPIOX_ODR(LCD_RD) = 0;                     \
  LCD_IO_Delay(LCD_IO_RW_DELAY);             \
  BITBAND_ACCESS(dt, 0) = GPIOX_IDR(LCD_D0); \
  BITBAND_ACCESS(dt, 1) = GPIOX_IDR(LCD_D1); \
  BITBAND_ACCESS(dt, 2) = GPIOX_IDR(LCD_D2); \
  BITBAND_ACCESS(dt, 3) = GPIOX_IDR(LCD_D3); \
  BITBAND_ACCESS(dt, 4) = GPIOX_IDR(LCD_D4); \
  BITBAND_ACCESS(dt, 5) = GPIOX_IDR(LCD_D5); \
  BITBAND_ACCESS(dt, 6) = GPIOX_IDR(LCD_D6); \
  BITBAND_ACCESS(dt, 7) = GPIOX_IDR(LCD_D7); \
  GPIOX_ODR(LCD_RD) = 1;                     }
#endif
#endif

#define  LCD_DUMMY_READ {         \
  GPIOX_ODR(LCD_RD) = 0;          \
  LCD_IO_Delay(LCD_IO_RW_DELAY);  \
  GPIOX_ODR(LCD_RD) = 1;          }

#define  LCD_CMD8_WRITE(cmd)  {LCD_RS_CMD; LCD_DATA_WRITE(cmd); LCD_RS_DATA; }
#define  LCD_CMD16_WRITE(cmd) {LCD_RS_CMD; LCD_DATA_WRITE(cmd >> 8); LCD_DATA_WRITE(cmd); LCD_RS_DATA; }

#if TS_ADC == 1
#define  RCC_APB2ENR_ADCXEN  RCC_APB2ENR_ADC1EN
#define  ADCX  ADC1
#endif

#if TS_ADC == 2
#define  RCC_APB2ENR_ADCXEN  RCC_APB2ENR_ADC2EN
#define  ADCX  ADC2
#endif

#if TS_ADC == 3
#define  RCC_APB2ENR_ADCXEN  RCC_APB2ENR_ADC3EN
#define  ADCX  ADC3
#endif

//-----------------------------------------------------------------------------
#pragma GCC push_options
#pragma GCC optimize("O0")
void LCD_IO_Delay(volatile uint32_t c)
{
  while(c--);
}
#pragma GCC pop_options

//-----------------------------------------------------------------------------
void LCD_Delay(uint32_t Delay)
{
  HAL_Delay(Delay);
}

//-----------------------------------------------------------------------------
void LCD_IO_Bl_OnOff(uint8_t Bl)
{
  #if (GPIOX_PORTNUM(LCD_BL) >= 1) && (GPIOX_PORTNUM(LCD_BL) <= 12)
//  #ifdef LCD_BL
  if(Bl)
    GPIOX_ODR(LCD_BL) = LCD_BLON;
  else
    GPIOX_ODR(LCD_BL) = 1 - LCD_BLON;
  #endif
}

//-----------------------------------------------------------------------------
void LCD_IO_Init(void)
{
  RCC->AHB1ENR |= (GPIOX_CLOCK(LCD_CS) | GPIOX_CLOCK(LCD_RS) | GPIOX_CLOCK(LCD_WR) | GPIOX_CLOCK(LCD_RD) | GPIOX_CLOCK(LCD_RST) |
                   GPIOX_CLOCK(LCD_D0) | GPIOX_CLOCK(LCD_D1) | GPIOX_CLOCK(LCD_D2) | GPIOX_CLOCK(LCD_D3) |
                   GPIOX_CLOCK(LCD_D4) | GPIOX_CLOCK(LCD_D5) | GPIOX_CLOCK(LCD_D6) | GPIOX_CLOCK(LCD_D7));

  // disable the LCD
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  LCD_RS_DATA;                          // RS = 1
  GPIOX_ODR(LCD_WR) = 1;                // WR = 1
  GPIOX_ODR(LCD_RD) = 1;                // RD = 1
  LCD_RST_OFF;                          // RST = 1

  GPIOX_MODER(MODE_OUT, LCD_CS);
  GPIOX_MODER(MODE_OUT, LCD_RS);
  GPIOX_MODER(MODE_OUT, LCD_WR);
  GPIOX_MODER(MODE_OUT, LCD_RD);
  GPIOX_MODER(MODE_OUT, LCD_RST);

  LCD_DATA_DIRWRITE;                    // adatl�bak kimenetre �llit�sa

  // GPIO sebess�g MAX
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_CS);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_RS);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_WR);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_RD);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_RST);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D0);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D1);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D2);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D3);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D4);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D5);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D6);
  GPIOX_OSPEEDR(MODE_SPD_VHIGH, LCD_D7);

  /* Set or Reset the control line */
  LCD_Delay(1);
  LCD_RST_ON;                           // RST = 0
  LCD_Delay(1);
  LCD_RST_OFF;                          // RST = 1
  LCD_Delay(1);

  #ifdef ADCX
  RCC->APB2ENR |= RCC_APB2ENR_ADCXEN;
  ADCX->CR1 = ADC_CR1_DISCEN;
  ADCX->CR2 = ADC_CR2_ADON;

  #if TS_XM_ADCCH >= 10
  ADCX->SMPR1 |= TS_SAMPLETIME << (3 * (TS_XM_ADCCH - 10));
  #else
  ADCX->SMPR2 |= TS_SAMPLETIME << (3 * (TS_XM_ADCCH));
  #endif

  #if TS_YP_ADCCH >= 10
  ADCX->SMPR1 |= TS_SAMPLETIME << (3 * (TS_YP_ADCCH - 10));
  #else
  ADCX->SMPR2 |= TS_SAMPLETIME << (3 * (TS_YP_ADCCH));
  #endif
  #endif  // #ifdef ADCX

  LCD_TS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd8(uint8_t Cmd)
{
  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);
  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd16(uint16_t Cmd)
{
  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);
  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteData8(uint8_t Data)
{
  LCD_CS_ON;
  LCD_DATA_WRITE(Data);
  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteData16(uint16_t Data)
{
  LCD_CS_ON;
  LCD_DATA_WRITE(Data >> 8);
  LCD_DATA_WRITE(Data);
  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd8DataFill16(uint8_t Cmd, uint16_t Data, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(Data >> 8);
    LCD_DATA_WRITE(Data);
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd8MultipleData8(uint8_t Cmd, uint8_t *pData, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(*pData);
    pData ++;
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd8MultipleData16(uint8_t Cmd, uint16_t *pData, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(*pData >> 8);
    LCD_DATA_WRITE(*pData);
    pData ++;
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd16DataFill16(uint16_t Cmd, uint16_t Data, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(Data >> 8);
    LCD_DATA_WRITE(Data);
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd16MultipleData8(uint16_t Cmd, uint8_t *pData, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(*pData);
    pData ++;
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_WriteCmd16MultipleData16(uint16_t Cmd, uint16_t *pData, uint32_t Size)
{
  uint32_t counter;

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_WRITE(*pData >> 8);
    LCD_DATA_WRITE(*pData);
    pData ++;
  }

  LCD_CS_OFF;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd8MultipleData8(uint8_t Cmd, uint8_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  d;

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(d);
    *pData = d;
    pData++;
  }

  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd8MultipleData16(uint8_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  dl, dh;

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(dh);
    LCD_DATA_READ(dl);
    *pData = (dh << 8) | dl;
    pData++;
  }
  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd8MultipleData24to16(uint8_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  rgb888[3];

  LCD_CS_ON;
  LCD_CMD8_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(rgb888[0]);
    LCD_DATA_READ(rgb888[1]);
    LCD_DATA_READ(rgb888[2]);
    *pData = ((rgb888[0] & 0b11111000) << 8 | (rgb888[1] & 0b11111100) << 3 | rgb888[2] >> 3);
    pData++;
  }
  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd16MultipleData8(uint16_t Cmd, uint8_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  d;

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(d);
    *pData = d;
    pData++;
  }

  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd16MultipleData16(uint16_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  dl, dh;

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(dh);
    LCD_DATA_READ(dl);
    *pData = (dh << 8) | dl;
    pData++;
  }
  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//-----------------------------------------------------------------------------
void LCD_IO_ReadCmd16MultipleData24to16(uint16_t Cmd, uint16_t *pData, uint32_t Size, uint32_t DummySize)
{
  uint32_t counter;
  uint8_t  rgb888[3];

  LCD_CS_ON;
  LCD_CMD16_WRITE(Cmd);

  LCD_DATA_DIRREAD;

  for (counter = DummySize; counter != 0; counter--)
  {
    LCD_DUMMY_READ;
  }

  for (counter = Size; counter != 0; counter--)
  {
    LCD_DATA_READ(rgb888[0]);
    LCD_DATA_READ(rgb888[1]);
    LCD_DATA_READ(rgb888[2]);
    *pData = ((rgb888[0] & 0b11111000) << 8 | (rgb888[1] & 0b11111100) << 3 | rgb888[2] >> 3);
    pData++;
  }
  LCD_CS_OFF;
  LCD_DATA_DIRWRITE;
}

//=============================================================================
#ifdef ADCX
// CS = 1, X+ = 0, X- = 0; Y+ = in PU, Y- = in PU
uint8_t TS_IO_DetectToch(void)
{
  uint8_t  ret;
  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  GPIOX_MODER(MODE_DIGITAL_INPUT, TS_YP);// YP = D_INPUT
  GPIOX_MODER(MODE_DIGITAL_INPUT, TS_YM);// YM = D_INPUT
  GPIOX_ODR(TS_XP) = 0;                 // XP = 0
  GPIOX_ODR(TS_XM) = 0;                 // XM = 0

  // Felhuzo ell. be
  GPIOX_PUPDR(MODE_PU_UP, TS_YP);

  LCD_IO_Delay(TS_AD_DELAY);

  if(GPIOX_IDR(TS_YP))
    ret = 0;                            // Touchscreen nincs megnyomva
  else
    ret = 1;                            // Touchscreen meg van nyomva

  // Felhuzo ell. ki
  GPIOX_PUPDR(MODE_PU_NONE, TS_YP);

  GPIOX_ODR(TS_XP) = 1;                 // XP = 1
  GPIOX_ODR(TS_XM) = 1;                 // XM = 1
  GPIOX_MODER(MODE_OUT, TS_YP);         // YP = OUT
  GPIOX_MODER(MODE_OUT, TS_YM);         // YM = OUT

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 0;                // CS = 0
  #endif
  return ret;
}

//-----------------------------------------------------------------------------
// X poz analog olvas�sa
uint16_t TS_IO_GetX(void)
{
  uint16_t ret;
  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  GPIOX_MODER(MODE_DIGITAL_INPUT, TS_YM);// YM = D_INPUT
  GPIOX_MODER(MODE_ANALOG_INPUT, TS_YP); // YP = AN_INPUT
  GPIOX_ODR(TS_XP) = 0;                 // XP = 0
  GPIOX_ODR(TS_XM) = 1;                 // XM = 1

  ADCX->SQR3 = TS_YP_ADCCH;
  LCD_IO_Delay(TS_AD_DELAY);
  ADCX->CR2 |= ADC_CR2_SWSTART;
  while(!(ADCX->SR & ADC_SR_EOC));
  ret = ADCX->DR;

  GPIOX_ODR(TS_XP) = 1;                 // XP = 1
  GPIOX_MODER(MODE_OUT, TS_YP);
  GPIOX_MODER(MODE_OUT, TS_YM);

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 0;                // CS = 0
  #endif
  return ret;
}

//-----------------------------------------------------------------------------
// Y poz analog olvas�sa
uint16_t TS_IO_GetY(void)
{
  uint16_t ret;

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  GPIOX_MODER(MODE_DIGITAL_INPUT, TS_XP);// XP = D_INPUT
  GPIOX_MODER(MODE_ANALOG_INPUT, TS_XM); // XM = AN_INPUT
  GPIOX_ODR(TS_YM) = 0;                 // YM = 0
  GPIOX_ODR(TS_YP) = 1;                 // YP = 1

  ADCX->SQR3 = TS_XM_ADCCH;
  LCD_IO_Delay(TS_AD_DELAY);
  ADCX->CR2 |= ADC_CR2_SWSTART;
  while(!(ADCX->SR & ADC_SR_EOC));
  ret = ADCX->DR;

  GPIOX_ODR(TS_YM) = 1;                 // YM = 1
  GPIOX_MODER(MODE_OUT, TS_XP);
  GPIOX_MODER(MODE_OUT, TS_XM);

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 0;                // CS = 0
  #endif

  return ret;
}

//-----------------------------------------------------------------------------
// Z1 poz analog olvas�sa
uint16_t TS_IO_GetZ1(void)
{
  uint16_t ret;
  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  GPIOX_MODER(MODE_ANALOG_INPUT, TS_XM); // XM = AN_INPUT
  GPIOX_MODER(MODE_ANALOG_INPUT, TS_YP); // YP = AN_INPUT
  GPIOX_ODR(TS_XP) = 0;                 // XP = 0
  GPIOX_ODR(TS_YM) = 1;                 // YM = 1

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  ADCX->SQR3 = TS_YP_ADCCH;
  LCD_IO_Delay(TS_AD_DELAY);
  ADCX->CR2 |= ADC_CR2_SWSTART;
  while(!(ADCX->SR & ADC_SR_EOC));
  ret = ADCX->DR;

  GPIOX_ODR(TS_XP) = 1;                 // XP = 1
  GPIOX_MODER(MODE_OUT, TS_XM);
  GPIOX_MODER(MODE_OUT, TS_YP);

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 0;                // CS = 0
  #endif

  return ret;
}

//-----------------------------------------------------------------------------
// Z2 poz analog olvas�sa
uint16_t TS_IO_GetZ2(void)
{
  uint16_t ret;
  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 1;                // CS = 1
  #endif

  GPIOX_MODER(MODE_ANALOG_INPUT, TS_XM); // XM = AN_INPUT
  GPIOX_MODER(MODE_ANALOG_INPUT, TS_YP); // YP = AN_INPUT
  GPIOX_ODR(TS_XP) = 0;                 // XP = 0
  GPIOX_ODR(TS_YM) = 1;                 // YM = 1

  ADCX->SQR3 = TS_XM_ADCCH;
  LCD_IO_Delay(TS_AD_DELAY);
  ADCX->CR2 |= ADC_CR2_SWSTART;
  while(!(ADCX->SR & ADC_SR_EOC));
  ret = ADCX->DR;

  GPIOX_ODR(TS_XP) = 1;                 // XP = 1
  GPIOX_MODER(MODE_OUT, TS_XM);
  GPIOX_MODER(MODE_OUT, TS_YP);

  #ifdef LCD_CS_OPT
  GPIOX_ODR(LCD_CS) = 0;                // CS = 0
  #endif

  return ret;
}

#else  // #ifdef ADCX
__weak uint8_t   TS_IO_DetectToch(void) { return 0;}
__weak uint16_t  TS_IO_GetX(void)       { return 0;}
__weak uint16_t  TS_IO_GetY(void)       { return 0;}
__weak uint16_t  TS_IO_GetZ1(void)      { return 0;}
__weak uint16_t  TS_IO_GetZ2(void)      { return 0;}
#endif // #else ADCX
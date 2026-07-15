/* USER CODE BEGIN Header */
#ifndef SRC_IRDA_H_
#define SRC_IRDA_H_
/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

#define LCD_DATA_PORT	GPIOD
#define LCD_D0			GPIO_PIN_14
#define LCD_D1			GPIO_PIN_15
#define LCD_D2			GPIO_PIN_12
#define LCD_D3			GPIO_PIN_13
#define LCD_D4			GPIO_PIN_10
#define LCD_D5			GPIO_PIN_11
#define LCD_D6			GPIO_PIN_8
#define LCD_D7			GPIO_PIN_9

#define LCD_CONTROL_PORT	GPIOG
#define LCD_RS			GPIO_PIN_3
#define LCD_RW			GPIO_PIN_2
#define LCD_E			GPIO_PIN_4

void LCD_Init();
void LCD_GotoXY(char row, char col);
void LCD_Write_String(char *s);
void LCD_Write_Char(char c);
void LCD_Send_Command(unsigned char x);

#endif

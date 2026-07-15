/*
 * lcd1602.c
 *
 *  Created on: Oct 31, 2024
 *      Author: ngola
 */

#include "lcd1602.h"
#include "stm32f4xx_hal.h"
#include "string.h"

void Delay_lcd(int interval)
{
	int j;
	for(j=0;j<interval;j++);
}

void LCD_DatabusWrite(char x)
{
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D0, (x >> 0) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D1, (x >> 1) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D2, (x >> 2) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D3, (x >> 3) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D4, (x >> 4) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D5, (x >> 5) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D6, (x >> 6) & 0x01);
	HAL_GPIO_WritePin(LCD_DATA_PORT, LCD_D7, (x >> 7) & 0x01);
}

//Ham gui mot lenh xuong LCD
void LCD_Send_Command(unsigned char x)
{
	LCD_DatabusWrite(x);	// command register
	//LCD_RS=0;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_RS, 0);
	//LCD_RW=0;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_RW, 0);
	//LCD_EN=1;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 1);
	Delay_lcd(10000);
	//LCD_EN=0;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 0);
	Delay_lcd(10000);
	//LCD_EN=1;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 1);
}

//Ham de LCD hien thi mot ky tu
void LCD_Write_Char(char c)
{
	LCD_DatabusWrite(c);
	//LCD_RS=1; 		//data register
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_RS, 1);
	//LCD_RW=0;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_RW, 0);
	//LCD_EN=1;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 1);
	Delay_lcd(10000);
	//LCD_EN=0;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 0);
	Delay_lcd(10000);
	//LCD_EN=1;
	HAL_GPIO_WritePin(LCD_CONTROL_PORT, LCD_E, 1);
}

void LCD_Init()
{
	LCD_Send_Command(0x38); //Chon che do 8 bit, 2 hang cho LCD
	LCD_Send_Command(0x0E); //Bat hien thi, nhap nhay con tro
	LCD_Send_Command(0x01); //Xoa man hinh
	LCD_Send_Command(0x80); //Ve dau dong
}

void LCD_GotoXY(char row, char col)
{
    char i;
    if (row == 2){
        LCD_Send_Command(0xC0);      //cursor to fist col in row 2
    } else {
       	LCD_Send_Command(0x80);      //cursor to fist col in row 1 (default)
    }
	for (i = 0; i < col; i++)
        LCD_Send_Command(0x14);      //cursor to fist col in row 1 (default)
}


void LCD_Write_String(char *s)
{
	int length;
	length=strlen(s); //Lay do dai xau
	while(length!=0)
	{
		LCD_Write_Char(*s); //Ghi ra LCD gia tri duoc tro boi con tro
		s++; //Tang con tro
		length--;
	}
}


/**
 * Mifare MFRC522 RFID Card reader
 * It works on 13.56 MHz.
 *
 * This library uses SPI for driving MFRC255 chip.
 *
 *	@author 	Tilen Majerle
 *	@email		tilen@majerle.eu
 *	@website	http://stm32f4-discovery.com
 *	@link		http://stm32f4-discovery.com/2014/07/library-23-read-rfid-tag-mfrc522-stm32f4xx-devices/
 *	@version 	v1.0
 *	@ide		Keil uVision
 *	@license	GNU GPL v3
 *	
 * |----------------------------------------------------------------------
 * | Copyright (C) Tilen Majerle, 2014
 * | Modification by TrungNL to work with STM32F429I and HAL (CubeF4 1.28.1)
 * | All dependencies to other TM libs such as TM_SPI are removed
 * | 
 * | This program is free software: you can redistribute it and/or modify
 * | it under the terms of the GNU General Public License as published by
 * | the Free Software Foundation, either version 3 of the License, or
 * | any later version.
 * |  
 * | This program is distributed in the hope that it will be useful,
 * | but WITHOUT ANY WARRANTY; without even the implied warranty of
 * | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * | GNU General Public License for more details.
 * | 
 * | You should have received a copy of the GNU General Public License
 * | along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * |----------------------------------------------------------------------
 * 	
 * MF RC522 Default pinout
 * 
 * 		MFRC522		STM32F429ZIT6	DESCRIPTION
 *		SS(SDA)		PE4				Chip select for SPI
 *		SCK			PE2				Serial Clock for SPI
 *		MISO		PE5				Master In Slave Out for SPI
 *		MOSI		PE6				Master Out Slave In for SPI
 *		GND			GND				Ground
 *		VCC			3.3V			3.3V power
 *		RST			3.3V			Reset pin
 *		
 */
/**
 * Mifare MFRC522 RFID Card reader
 * Modified to work with SPI1 (PA4, PA5, PA6, PA7) and RST (PD7)
 */
#ifndef TM_MFRC522_H
#define TM_MFRC522_H 100

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

// Đổi sang sử dụng bộ SPI1
#ifndef MFRC522_SPI
#define MFRC522_SPI						SPI1
#endif

/* Cấu hình chân CS (NSS) sang chân PA4 */
#ifndef MFRC522_CS_PIN
#define MFRC522_CS_PORT					GPIOA
#define MFRC522_CS_PIN					GPIO_PIN_4
#endif

/**
 * Status enumeration
 */
typedef enum {
	MI_OK = 0,
	MI_NOTAGERR,
	MI_ERR
} TM_MFRC522_Status_t;

// Macro kéo chân CS xuống thấp và kéo lên cao (Sửa tương ứng với PA4)
#define MFRC522_CS_LOW					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define MFRC522_CS_HIGH					HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)

/* MFRC522 Commands */
#define PCD_IDLE						0x00
#define PCD_AUTHENT						0x0E
#define PCD_RECEIVE						0x08
#define PCD_TRANSMIT					0x04
#define PCD_TRANSCEIVE					0x0C
#define PCD_RESETPHASE					0x0F
#define PCD_CALCCRC						0x03

/* Mifare_One card command word */
#define PICC_REQIDL						0x26
#define PICC_REQALL						0x52
#define PICC_ANTICOLL					0x93
#define PICC_SElECTTAG					0x93
#define PICC_AUTHENT1A					0x60
#define PICC_AUTHENT1B					0x61
#define PICC_READ						0x30
#define PICC_WRITE						0xA0
#define PICC_DECREMENT					0xC0
#define PICC_INCREMENT					0xC1
#define PICC_RESTORE					0xC2
#define PICC_TRANSFER					0xB0
#define PICC_HALT						0x50

/* MFRC522 Registers */
#define MFRC522_REG_RESERVED00			0x00    
#define MFRC522_REG_COMMAND				0x01    
#define MFRC522_REG_COMM_IE_N			0x02    
#define MFRC522_REG_DIV1_EN				0x03    
#define MFRC522_REG_COMM_IRQ			0x04    
#define MFRC522_REG_DIV_IRQ				0x05
#define MFRC522_REG_ERROR				0x06    
#define MFRC522_REG_STATUS1				0x07    
#define MFRC522_REG_STATUS2				0x08    
#define MFRC522_REG_FIFO_DATA			0x09
#define MFRC522_REG_FIFO_LEVEL			0x0A
#define MFRC522_REG_WATER_LEVEL			0x0B
#define MFRC522_REG_CONTROL				0x0C
#define MFRC522_REG_BIT_FRAMING			0x0D
#define MFRC522_REG_COLL				0x0E
#define MFRC522_REG_RESERVED01			0x0F
#define MFRC522_REG_RESERVED10			0x10
#define MFRC522_REG_MODE				0x11
#define MFRC522_REG_TX_MODE				0x12
#define MFRC522_REG_RX_MODE				0x13
#define MFRC522_REG_TX_CONTROL			0x14
#define MFRC522_REG_TX_AUTO				0x15
#define MFRC522_REG_TX_SELL				0x16
#define MFRC522_REG_RX_SELL				0x17
#define MFRC522_REG_RX_THRESHOLD		0x18
#define MFRC522_REG_DEMOD				0x19
#define MFRC522_REG_RESERVED11			0x1A
#define MFRC522_REG_RESERVED12			0x1B
#define MFRC522_REG_MIFARE				0x1C
#define MFRC522_REG_RESERVED13			0x1D
#define MFRC522_REG_RESERVED14			0x1E
#define MFRC522_REG_SERIALSPEED			0x1F
#define MFRC522_REG_RESERVED20			0x20  
#define MFRC522_REG_CRC_RESULT_M		0x21
#define MFRC522_REG_CRC_RESULT_L		0x22
#define MFRC522_REG_RESERVED21			0x23
#define MFRC522_REG_MOD_WIDTH			0x24
#define MFRC522_REG_RESERVED22			0x25
#define MFRC522_REG_RF_CFG				0x26
#define MFRC522_REG_GS_N				0x27
#define MFRC522_REG_CWGS_PREG			0x28
#define MFRC522_REG__MODGS_PREG			0x29
#define MFRC522_REG_T_MODE				0x2A
#define MFRC522_REG_T_PRESCALER			0x2B
#define MFRC522_REG_T_RELOAD_H			0x2C
#define MFRC522_REG_T_RELOAD_L			0x2D
#define MFRC522_REG_T_COUNTER_VALUE_H	0x2E
#define MFRC522_REG_T_COUNTER_VALUE_L	0x2F
#define MFRC522_REG_RESERVED30			0x30
#define MFRC522_REG_TEST_SEL1			0x31
#define MFRC522_REG_TEST_SEL2			0x32
#define MFRC522_REG_TEST_PIN_EN			0x33
#define MFRC522_REG_TEST_PIN_VALUE		0x34
#define MFRC522_REG_TEST_BUS			0x35
#define MFRC522_REG_AUTO_TEST			0x36
#define MFRC522_REG_VERSION				0x37
#define MFRC522_REG_ANALOG_TEST			0x38
#define MFRC522_REG_TEST_ADC1			0x39  
#define MFRC522_REG_TEST_ADC2			0x3A   
#define MFRC522_REG_TEST_ADC0			0x3B   
#define MFRC522_REG_RESERVED31			0x3C   
#define MFRC522_REG_RESERVED32			0x3D
#define MFRC522_REG_RESERVED33			0x3E   
#define MFRC522_REG_RESERVED34			0x3F
#define MFRC522_DUMMY					0x00
#define MFRC522_MAX_LEN					16

/**
 * Public functions
 */
void TM_MFRC522_Init(void);
TM_MFRC522_Status_t TM_MFRC522_Check(uint8_t* id);
TM_MFRC522_Status_t TM_MFRC522_Compare(uint8_t* CardID, uint8_t* CompareID);

/**
 * Private functions
 */
void TM_MFRC522_InitPins(void);
void TM_MFRC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t TM_MFRC522_ReadRegister(uint8_t addr);
void TM_MFRC522_SetBitMask(uint8_t reg, uint8_t mask);
void TM_MFRC522_ClearBitMask(uint8_t reg, uint8_t mask);
void TM_MFRC522_AntennaOn(void);
void TM_MFRC522_AntennaOff(void);
void TM_MFRC522_Reset(void);
TM_MFRC522_Status_t TM_MFRC522_Request(uint8_t reqMode, uint8_t* TagType);
TM_MFRC522_Status_t TM_MFRC522_ToCard(uint8_t command, uint8_t* sendData, uint8_t sendLen, uint8_t* backData, uint16_t* backLen);
TM_MFRC522_Status_t TM_MFRC522_Anticoll(uint8_t* serNum);
void TM_MFRC522_CalculateCRC(uint8_t* pIndata, uint8_t len, uint8_t* pOutData);
uint8_t TM_MFRC522_SelectTag(uint8_t* serNum);
TM_MFRC522_Status_t TM_MFRC522_Auth(uint8_t authMode, uint8_t BlockAddr, uint8_t* Sectorkey, uint8_t* serNum);
TM_MFRC522_Status_t TM_MFRC522_Read(uint8_t blockAddr, uint8_t* recvData);
TM_MFRC522_Status_t TM_MFRC522_Write(uint8_t blockAddr, uint8_t* writeData);
void TM_MFRC522_Halt(void) ;

#endif

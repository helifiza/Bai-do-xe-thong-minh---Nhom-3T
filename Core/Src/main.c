/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MAX_CARDS 10          // Số lượng thẻ tối đa hệ thống có thể lưu
#define RX_BUF_SIZE 60        // Kích thước bộ đệm nhận UART
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim2;

// Cơ sở dữ liệu danh sách thẻ hợp lệ
uint8_t AllowedCards[MAX_CARDS][5] = {
    {0x56, 0x62, 0x2E, 0x02, 0x18},  // Thẻ Master mặc định ban đầu tại vị trí [0]
    {0xF0, 0x94, 0xD8, 0x5C, 0xE0},
	{0x40, 0x27, 0xC4, 0x41, 0xE2},
	{0xA4, 0x73, 0xC4, 0x41, 0x52}
};
uint8_t current_card_count = 4;

uint8_t str[5]; // Lưu UID thẻ vừa quét

// Biến điều khiển nhận bằng NGẮT UART
uint8_t rx_byte = 0;                  // Biến nhận từng byte một từ ngắt
char rx_buffer[RX_BUF_SIZE];
uint16_t rx_index = 0;
volatile uint8_t command_ready = 0;  // Sử dụng volatile vì biến thay đổi trong ISR

// Biến lưu khoảng cách cho cảm biến 2 và cảm biến 3
volatile uint32_t distance2 = 0;
volatile uint32_t distance3 = 0;

// Các biến phục vụ đo EXTI cho cảm biến 2
volatile uint32_t echo2_start_time = 0, echo2_end_time = 0;

// Các biến phục vụ đo EXTI cho cảm biến 3
volatile uint32_t echo3_start_time = 0, echo3_end_time = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
uint8_t Is_Card_Allowed(uint8_t *uid);
void Process_UART_Command(char *input);
uint8_t IsValidHexCard(char *str_hex);
void Control_Servo(uint8_t angle);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void delay_us(uint16_t us) {
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    while (__HAL_TIM_GET_COUNTER(&htim2) < us);
}



// Hàm Callback xử lý ngắt nhận UART (Tự động gọi khi có 1 byte truyền đến)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Nếu nhận được ký tự xuống dòng, hoàn tất lệnh
        if (rx_byte == '\n' || rx_byte == '\r') {
            if (rx_index > 0) {
                rx_buffer[rx_index] = '\0';
                command_ready = 1;
            }
        } else {
            // Đưa dữ liệu vào hàng đợi đệm nếu chưa xử lý xong lệnh cũ
            if (command_ready == 0) {
                if (rx_index < RX_BUF_SIZE - 1) {
                    rx_buffer[rx_index++] = rx_byte;
                } else {
                    rx_index = 0; // Tràn đệm tự động xóa
                }
            }
        }
        // Tiếp tục kích hoạt chế độ ngắt chờ byte tiếp theo
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

// Hàm kiểm tra định dạng độ dài và tính hợp lệ của chuỗi mã thẻ HEX
uint8_t IsValidHexCard(char *str_hex) {
    if (strlen(str_hex) != 10) return 0;
    for (int i = 0; i < 10; i++) {
        if (!isxdigit((unsigned char)str_hex[i])) return 0;
    }
    return 1;
}

// Hàm kiểm tra xem UID thẻ vừa quét có nằm trong danh sách được phép không
uint8_t Is_Card_Allowed(uint8_t *uid) {
    for (uint8_t i = 0; i < current_card_count; i++) {
        if (TM_MFRC522_Compare(uid, AllowedCards[i]) == MI_OK) {
            return 1;
        }
    }
    return 0;
}

void Process_UART_Command(char *input) {
    char response[150];
    int tx_len;

    char *cmd = strtok(input, " \t\r\n");
    if (cmd == NULL) return;

    for(int i = 0; cmd[i]; i++) cmd[i] = tolower((unsigned char)cmd[i]);

    // ================= LỆNH ADD THẺ RFID =================
    if (strcmp(cmd, "add") == 0) {
        char *arg = strtok(NULL, " \t\r\n");

        if (arg != NULL && IsValidHexCard(arg)) {
            for(int i = 0; arg[i]; i++) arg[i] = toupper((unsigned char)arg[i]);

            if (current_card_count < MAX_CARDS) {
                uint8_t temp_uid[5];
                char hex_part[3];
                hex_part[2] = '\0';

                for (int i = 0; i < 5; i++) {
                    hex_part[0] = arg[i * 2];
                    hex_part[1] = arg[i * 2 + 1];
                    temp_uid[i] = (uint8_t)strtol(hex_part, NULL, 16);
                }

                uint8_t duplicated = 0;
                for (uint8_t i = 0; i < current_card_count; i++) {
                    if (TM_MFRC522_Compare(temp_uid, AllowedCards[i]) == MI_OK) {
                        duplicated = 1;
                        break;
                    }
                }

                if (duplicated) {
                    tx_len = sprintf(response, "\r\n[SYSTEM] WARNING: The %s da ton tai tu truoc!\r\n", arg);
                } else {
                    memcpy(AllowedCards[current_card_count], temp_uid, 5);
                    current_card_count++;
                    tx_len = sprintf(response, "\r\n[SYSTEM] SUCCESS: Da add the %s vao bo nho! (%d/%d)\r\n", arg, current_card_count, MAX_CARDS);
                }
            } else {
                tx_len = sprintf(response, "\r\n[SYSTEM] ERROR: Bo nho he thong da day!\r\n");
            }
        } else {
            tx_len = sprintf(response, "\r\n[SYSTEM] ERROR: Cu phap sai! Ma the phai du 10 ky tu HEX. Vi du: add A473C44152\r\n");
        }
        HAL_UART_Transmit(&huart1, (uint8_t*)response, tx_len, 100);
    }
    // ================= LỆNH DEBUG SERVO MỚI =================
    else if (strcmp(cmd, "setservo") == 0) {
        char *arg = strtok(NULL, " \t\r\n");
        if (arg != NULL) {
            int angle = atoi(arg);
            if (angle >= 0 && angle <= 180) {
                // Gọi hàm điều khiển và lấy giá trị CCR thực tế sau khi tính
                Control_Servo((uint8_t)angle);
                uint32_t current_ccr = __HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1);

                tx_len = sprintf(response, "\r\n[SERVO_DEBUG] Thuc thi lenh quay: %d do | Gia tri ghi vao CCR: %lu\r\n", angle, current_ccr);
            } else {
                tx_len = sprintf(response, "\r\n[SERVO_DEBUG] ERROR: Goc nhap vao phai tu 0 den 180!\r\n");
            }
        } else {
            tx_len = sprintf(response, "\r\n[SERVO_DEBUG] ERROR: Thieu tham so goc. Cu phap: setservo <goc>\r\n");
        }
        HAL_UART_Transmit(&huart1, (uint8_t*)response, tx_len, 100);
    }
    else {
        tx_len = sprintf(response, "\r\n[SYSTEM] ERROR: Lenh '%s' khong duoc ho tro! Dung: add <Ma_The> hoac setservo <Goc>\r\n", cmd);
        HAL_UART_Transmit(&huart1, (uint8_t*)response, tx_len, 100);
    }
}



void UID_To_String(uint8_t *uid, char *out_str) {
    sprintf(out_str, "%02X%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3], uid[4]);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	typedef enum {
	    DOOR_IDLE,
	    DOOR_OPENING_VALID,
	    DOOR_OPENING_INVALID
	} DoorState_t;

	DoorState_t door_state = DOOR_IDLE;
	uint32_t door_timer = 0;
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(50);

    HAL_TIM_Base_Start(&htim2);

	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

	Control_Servo(0);

	LCD_Init();
    HAL_Delay(10);
    LCD_GotoXY(1, 0);
    LCD_Write_String("SMART PARKING 3T");
    LCD_GotoXY(2, 0);
    LCD_Write_String("Scan Your Card...");

    TM_MFRC522_Init();

    // KÍCH HOẠT NHẬN NGẮT UART TRƯỚC KHI VÀO WHILE(1)
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    char start_msg[] = "He thong khoi dong!\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)start_msg, sizeof(start_msg) - 1, 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	  uint32_t last_print_time = 0;
	  uint32_t last_trigger_time = 0;
	  uint8_t sensor_toggle = 0;

	  while (1)
	  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		char lcd_buf[32];
		char tx_buf[120];
		int tx_len;

		// =================================================================
		// XỬ LÝ LỆNH UART TỪ NGẮT ĐẨY RA
		// =================================================================
		if (command_ready) {
			Process_UART_Command(rx_buffer);

			// Xóa bộ đệm và hạ cờ an toàn
			memset(rx_buffer, 0, RX_BUF_SIZE);
			rx_index = 0;
			command_ready = 0;
		}

		// =================================================================
		// 1. KÍCH HOẠT ĐO SIÊU ÂM LUÂN PHIÊN
		// =================================================================
		if (HAL_GetTick() - last_trigger_time >= 60) {
			last_trigger_time = HAL_GetTick();
			if (sensor_toggle == 0) {
				Trigger_Sensor2();
				sensor_toggle = 1;
			} else {
				Trigger_Sensor3();
				sensor_toggle = 0;
			}
		}

		// =================================================================
		// 2. LOGIC ĐIỀU KHIỂN LED VÀ TÍNH SỐ SLOT OCCUPIED
		// =================================================================
		uint8_t occupied_slots = 0;

		if (distance2 > 0 && distance2 <= 5) {
			HAL_GPIO_WritePin(GPIOD, LED_RED2_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, LED_GREEN2_Pin, GPIO_PIN_RESET);
			occupied_slots++;
		} else {
			HAL_GPIO_WritePin(GPIOD, LED_RED2_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, LED_GREEN2_Pin, GPIO_PIN_SET);
		}

		if (distance3 > 0 && distance3 <= 5) {
			HAL_GPIO_WritePin(GPIOD, LED_RED3_Pin, GPIO_PIN_SET);
			HAL_GPIO_WritePin(GPIOD, LED_GREEN3_Pin, GPIO_PIN_RESET);
			occupied_slots++;
		} else {
			HAL_GPIO_WritePin(GPIOD, LED_RED3_Pin, GPIO_PIN_RESET);
			HAL_GPIO_WritePin(GPIOD, LED_GREEN3_Pin, GPIO_PIN_SET);
		}

		// =================================================================
		// 3. KIỂM TRA QUÉT THẺ RFID RC522 & QUẢN LÝ CỬA
		// =================================================================
		if (door_state == DOOR_IDLE) {
		if (TM_MFRC522_Check(str) == MI_OK) {
			HAL_GPIO_WritePin(GPIOB, BUZZER_Pin, GPIO_PIN_SET);
			HAL_Delay(50);
			HAL_GPIO_WritePin(GPIOB, BUZZER_Pin, GPIO_PIN_RESET);

			UID_To_String(str, lcd_buf);
			tx_len = sprintf(tx_buf, "The quet! UID: %s\r\n", lcd_buf);
			HAL_UART_Transmit(&huart1, (uint8_t*)tx_buf, tx_len, 100);

			// Kiểm tra xem bãi xe có bị đầy không trước khi check thẻ
			if (occupied_slots >= 2) {
				tx_len = sprintf(tx_buf, "Ket qua: TU CHOI! Bai do xe da day (2/2).\r\n");
				HAL_UART_Transmit(&huart1, (uint8_t*)tx_buf, tx_len, 100);

				LCD_GotoXY(1, 0);
				LCD_Write_String("PARKING FULL !! ");
				LCD_GotoXY(2, 0);
				LCD_Write_String("Cannot Enter!   ");

				Buzzer_Beep(3, 80); // Kêu bíp cảnh báo lỗi bãi đầy
				door_state = DOOR_OPENING_INVALID; // Chuyển sang trạng thái chờ reset màn hình ngắn (1.5s)
				door_timer = HAL_GetTick();
			}
			// Nếu bãi xe còn chỗ mới kiểm tra tính hợp lệ của thẻ
			else if (Is_Card_Allowed(str)) {
				tx_len = sprintf(tx_buf, "Ket qua: THE HOP LE. Mo cua!\r\n");
				HAL_UART_Transmit(&huart1, (uint8_t*)tx_buf, tx_len, 100);

				LCD_GotoXY(1, 0);
				LCD_Write_String("ACCESS GRANTED  ");
				LCD_GotoXY(2, 0);
				LCD_Write_String("Welcome Home!   ");

				Control_Servo(90);
				door_state = DOOR_OPENING_VALID;
				door_timer = HAL_GetTick();
			}
			else {
				tx_len = sprintf(tx_buf, "Ket qua: THE KHONG HOP LE. Tu choi!\r\n");
				HAL_UART_Transmit(&huart1, (uint8_t*)tx_buf, tx_len, 100);

				LCD_GotoXY(1, 0);
				LCD_Write_String("ACCESS DENIED   ");
				LCD_GotoXY(2, 0);
				LCD_Write_String("Invalid Card!   ");

				Buzzer_Beep(3, 80);
				door_state = DOOR_OPENING_INVALID;
				door_timer = HAL_GetTick();
			}
		}
	}
		// =================================================================
		// 4. MÁY TRẠNG THÁI XỬ LÝ THỜI GIAN CHỜ CỬA TỰ ĐỘNG ĐÓNG & HIỂN THỊ
		// =================================================================
		switch (door_state) {
			case DOOR_OPENING_VALID:
				// Ép thanh ghi giữ nguyên giá trị mở cửa (90 độ -> CCR = 75)
				Control_Servo(90);

				if (HAL_GetTick() - door_timer >= 4500) {
					Control_Servo(0); // Hết 4.5 giây mới đóng cửa
					door_state = DOOR_IDLE;
					last_print_time = 0;
				}
				break;

			case DOOR_OPENING_INVALID:
				if (HAL_GetTick() - door_timer >= 1500) {
					door_state = DOOR_IDLE;
					last_print_time = 0;
				}
				break;

			case DOOR_IDLE:
			default:
				// Khi rảnh rỗi, luôn ép servo về góc 0 độ (CCR = 25)
				Control_Servo(0);

				if (HAL_GetTick() - last_print_time >= 1500) {
					last_print_time = HAL_GetTick();

					uint32_t active_ccr = __HAL_TIM_GET_COMPARE(&htim2, TIM_CHANNEL_1);

					tx_len = sprintf(tx_buf, "DEBUG -> S2: %02lucm | S3: %02lucm | Occupied: %d/2 | Cards: %d | Servo_CCR: %lu\r\n",
									 distance2, distance3, occupied_slots, current_card_count, active_ccr);
					HAL_UART_Transmit(&huart1, (uint8_t*)tx_buf, tx_len, 100);

					LCD_GotoXY(1, 0);
					LCD_Write_String("SCAN YOUR CARD..");

					sprintf(lcd_buf, "Occupied: %d/2   ", occupied_slots);
					LCD_GotoXY(2, 0);
					LCD_Write_String(lcd_buf);
				}
				break;
		}
		HAL_Delay(1);
	  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 89;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, TRIG2_Pin|TRIG3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |LED_GREEN1_Pin|LED_GREEN2_Pin|LED_GREEN3_Pin|LED_RED1_Pin
                          |LED_RED2_Pin|LED_RED3_Pin|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PC4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : TRIG2_Pin TRIG3_Pin */
  GPIO_InitStruct.Pin = TRIG2_Pin|TRIG3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : ECHO2_Pin ECHO3_Pin */
  GPIO_InitStruct.Pin = ECHO2_Pin|ECHO3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PD8 PD9 PD10 PD11
                           PD12 PD13 PD14 PD15
                           LED_GREEN1_Pin LED_GREEN2_Pin LED_GREEN3_Pin LED_RED1_Pin
                           LED_RED2_Pin LED_RED3_Pin PD7 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15
                          |LED_GREEN1_Pin|LED_GREEN2_Pin|LED_GREEN3_Pin|LED_RED1_Pin
                          |LED_RED2_Pin|LED_RED3_Pin|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pins : PG2 PG3 PG4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

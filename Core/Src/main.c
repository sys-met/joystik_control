/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ibus.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define PID_RATE 30
#define PID_INTERVAL 1000/PID_RATE
unsigned long nextPID = PID_INTERVAL;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
uint32_t micros(void);
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
float kp = 1;//0.2;//10.0/28;
float kp2 = 1;//0.2;
float ki = 0;//0.0001125;
float ki2 = 0.0001125;
float ki_view = 0.0;
float ki_view2 = 0.0;
float left_u = 0.0;
float right_u = 0.0;
//float target = 0.0;
float error = 0.0;
float error2 = 0.0;
long current_time;
long previously_time;
int16_t delta_time;
float integer_error = 0;
float integer_error2 = 0;
float dt_view = 0.0;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t ibus_data[IBUS_USER_CHANNELS];
volatile int rx_count = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void set_motor(int left, int right);
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  int16_t sayim1;
  int16_t sayim2;
  int16_t sayim1_prev = 0;
  int16_t sayim2_prev = 0;
  int16_t sayim1_current = 0;
  int16_t sayim2_current = 0;
  uint32_t t1, t2, dt;
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);  // ENA (sol motor)
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 0);
  //set_motor(0, 0);

  HAL_Delay(1000);
  ibus_init();
  ibus_resync();
  uint8_t byte;
  // İlk byte 0x20 olana kadar oku
  while (ibus_data == 0)
  {
      MX_USART1_UART_Init();
      ibus_init();
          // İkinci byte 0x40 mı diye kontrol
      if (ibus_data[2] != 0)
      {
              break; // Senkron bulundu
      }

  }
  ibus_soft_failsafe(ibus_data, 10);
  //if (ibus_data[2] == 0) {ibus_init();}
  if (ibus_data[1] != 1500){ibus_data[1] = 1500;}
  if (ibus_data[3] != 1500){ibus_data[3] = 1500;}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    ibus_read(ibus_data);

    ibus_soft_failsafe(ibus_data, 10); // if ibus is not updated, clear ibus data.

    // 1. Joystick verisini oku (örnek: CH2 steering, CH3 throttle)
    int throttle = ibus_data[1] - 1500; // CH3: ileri/geri
    int steering = ibus_data[3] - 1500; // CH2: sağ/sol

    // Ölü bölge (joystick ortası) +50 sürücü kör bolge
    if (abs(throttle) < 10) throttle = 0;
    if (abs(steering) < 10) steering = 0;

    // 2. Diferansiyel motor hızlarını hesapla / hedefleri motor sürücüsüne uygun aralığa getirmelisin
    int left_target = 2*(throttle + steering);// kumanda 500 ila -500 değerleri veriyor - sürücü pwm değeri -+1000 değerleri arası hedef gerekiyor.
    int right_target = 2*(throttle - steering);// sayım hedefim 270 ila 50 arasıdır

    sayim1 = __HAL_TIM_GET_COUNTER(&htim1);
    sayim2 = __HAL_TIM_GET_COUNTER(&htim3);
    t1 = micros();

    dt = (uint32_t)(t1-t2);
    dt_view = (float)dt*1.0e-6;
    sayim1_current = sayim1 - sayim1_prev;
    sayim2_current = sayim2 - sayim2_prev;
    current_time = HAL_GetTick();
    //target = 220;

    error = (float)left_target - (float)sayim1_current;
    integer_error = integer_error + error*(float)dt_view;

    error2 = (float)right_target - sayim2_current;
    integer_error2 = integer_error2 + error2*(float)dt_view;
    //u = kp*target;
    ki_view = ki*integer_error;
    left_u = kp*error + ki*integer_error;

    ki_view2 = ki2*integer_error2;
    right_u = kp2*error2 + ki2*integer_error2;


    //printf("Enkoder 1 Sayisi: %d\r\n", sayim1_current);
    //printf("Enkoder 2 Sayisi: %d\r\n", sayim2);
    //HAL_Delay(100);
    //HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6, GPIO_PIN_SET);
    //HAL_GPIO_WritePin(GPIOA,GPIO_PIN_7, GPIO_PIN_RESET);
    //__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1,(int16_t)u);
    sayim1_prev = sayim1;
    sayim2_prev = sayim2;
    t2 = t1;


    // 3. Motorları sür
    set_motor(left_u, right_u);
    //HAL_Delay(10);
    HAL_Delay(PID_INTERVAL);
    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*) ptr, len, HAL_MAX_DELAY);
    return len;
}

void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // DWT aktif et
    DWT->CYCCNT = 0;                                // Sayaç sıfırla
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Sayaç başlat
}

uint32_t micros(void)
{
    return (uint32_t)(DWT->CYCCNT / (SystemCoreClock / 1000000));
}
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  //rx_count++;
  if(huart == IBUS_UART)
    ibus_reset_failsafe();

  ibus_init();

}
void set_motor(int left, int right)
{
    // Limit PWM değerlerini (örnek: -1000 ile 1000 arası)
    if (left > 1000) left = 1000;
    if (left < -1000) left = -1000;
    if (right > 1000) right = 1000;
    if (right < -1000) right = -1000;

    // Sol motor yön
    if (left >= 0)
    {
        //HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);   // IN1
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET); // IN2
    }
    else
    {
        //HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
        left = -left;
    }

    // Sağ motor yön
    if (right >= 0)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);   // IN3
        //HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // IN4
    }
    else
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
        //HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);
        right = -right;
    }

    // PWM ayarla (örnek: TIM2, TIM_CHANNEL_1 ve 2 kullanılıyor)
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, left);  // ENA (sol motor)
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, right); // ENB (sağ motor)
}
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

#ifdef  USE_FULL_ASSERT
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

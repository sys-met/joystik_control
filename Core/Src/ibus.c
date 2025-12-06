
/*
 * flysky_ibus.h
 *
 *  Created on: Feb 4, 2021
 *      Author: mokhwasomssi
 */


#include "ibus.h"


/* Static variable */
static uint8_t uart_rx_buffer[IBUS_LENGTH] = {0};
static uint8_t fail_safe_flag = 0;


/* Main Functions */
void ibus_init()
{
  HAL_UART_Receive_DMA(IBUS_UART, uart_rx_buffer, 32);
}

bool ibus_read(uint16_t* ibus_data)
{
  if(!ibus_is_valid())
    return false;

  if(!ibus_checksum())
    return false;
  //ibus_resync();
  ibus_update(ibus_data);
  return true;
}

void ibus_resync(void)
{
    uint8_t byte;
    // İlk byte 0x20 olana kadar oku
    while (HAL_UART_Receive(&huart1, &byte, 1, 10) == HAL_OK)
    {
        if (byte == 0x20) // iBus frame başlangıcı
        {
            // İkinci byte 0x40 mı diye kontrol
            if (HAL_UART_Receive(&huart1, &byte, 1, 10) == HAL_OK && byte == 0x40)
            {
                break; // Senkron bulundu
            }
        }
    }
}
/* Sub Functions */
bool ibus_is_valid()
{
  // is it ibus?
  return (uart_rx_buffer[0] == IBUS_LENGTH && uart_rx_buffer[1] == IBUS_COMMAND40);//mesaj ibus protokolune uygunmudur
}

bool ibus_checksum()
{
  uint16_t checksum_cal = 0xffff;
  uint16_t checksum_ibus;

  for(int i = 0; i < 30; i++)
  {
    checksum_cal -= uart_rx_buffer[i];
  }

  checksum_ibus = uart_rx_buffer[31] << 8 | uart_rx_buffer[30]; // checksum value from ibus
  return (checksum_ibus == checksum_cal);
}

void ibus_update(uint16_t* ibus_data)
{
  for(int ch_index = 0, bf_index = 2; ch_index < IBUS_USER_CHANNELS; ch_index++, bf_index += 2)
  {
    ibus_data[ch_index] = uart_rx_buffer[bf_index + 1] << 8 | uart_rx_buffer[bf_index];
  }
}

/**
 * @note FS-A8S don't have fail safe feature, So make software fail-safe.
 */
void ibus_soft_failsafe(uint16_t* ibus_data, uint8_t fail_safe_max)
{
  fail_safe_flag++;

  if(fail_safe_max > fail_safe_flag)
    return;

  // Clear ibus data
  for(int i = 0; i < IBUS_USER_CHANNELS; i++)
    ibus_data[i] = 0;

  ibus_data[1] = 1500;
  ibus_data[3] = 1500;
  // Clear ibus buffer
  for(int j = 0; j < IBUS_LENGTH; j++)
    uart_rx_buffer[j] = 0;

  fail_safe_flag = 0;
  return;
}

/**
 * @note This function is located in HAL_UART_RxCpltCallback.
 */
void ibus_reset_failsafe()
{
    fail_safe_flag = 0; // flag reset
}

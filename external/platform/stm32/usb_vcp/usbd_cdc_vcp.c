/**
  ******************************************************************************
  * @file    usbd_cdc_vcp.c
  * @author  MCD Application Team
  * @version V1.2.0 (modified)
  * @date    09-November-2015
  * @brief   Generic media access Layer.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2015 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/*
 * This file has been heavily modified for uD3TN
 * Modifications Copyright (c) 2016 Felix Walter and the authors of uPCN
 */

/* Includes ------------------------------------------------------------------*/
#include <stm32f4xx.h>

#include "usb_conf.h"
#include "usb_dcd_int.h"
#include "usbd_cdc_vcp.h"
#include "usbd_cdc_core.h"
#include "usbd_conf.h"
#include "usbd_desc.h"

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#ifdef USB_OTG_HS_INTERNAL_DMA_ENABLED
	#ifdef __ICCARM__ /*!< IAR Compiler */
		#pragma data_alignment = 4
	#endif
#endif /* USB_OTG_HS_INTERNAL_DMA_ENABLED */

/* Private typedef -----------------------------------------------------------*/
typedef struct {
	uint32_t bitrate;
	uint8_t  format;
	uint8_t  paritytype;
	uint8_t  datatype;
} LINE_CODING;

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
LINE_CODING linecoding = {
	115200, /* baud rate */
	0x00,   /* stop bits - 1 */
	0x00,   /* no parity */
	0x08    /* 8 bits */
};

__ALIGN_BEGIN USB_OTG_CORE_HANDLE USB_OTG_dev __ALIGN_END;

/* User "hooks", see below */
USBD_Usr_cb_TypeDef USR_cb;

/* FreeRTOS I/O */
static QueueHandle_t os_rx_queue, os_tx_queue;
static BaseType_t xHigherPriorityTaskWoken;

/* Private function prototypes -----------------------------------------------*/
static uint16_t VCP_Init(void);
static uint16_t VCP_DeInit(void);
static uint16_t VCP_Ctrl(uint32_t cmd, uint8_t *buf, uint32_t len);
static uint16_t VCP_DataTx(
	uint8_t *buf, uint32_t max, uint32_t *len, uint8_t *has_more);
static uint16_t VCP_DataRx(uint8_t *buf, uint32_t len);

CDC_IF_Prop_TypeDef VCP_fops =
{
	VCP_Init,
	VCP_DeInit,
	VCP_Ctrl,
	VCP_DataTx,
	VCP_DataRx
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  VCP_Init
  *         Initializes the Media on the STM32
  * @retval result (USBD_OK in all cases)
  */
static uint16_t VCP_Init(void)
{
	return USBD_OK;
}

/**
  * @brief  VCP_DeInit
  *         DeInitializes the Media on the STM32
  * @retval result (USBD_OK in all cases)
  */
static uint16_t VCP_DeInit(void)
{
	return USBD_OK;
}

/**
  * @brief  VCP_Ctrl
  *         Manage the CDC class requests
  * @param  cmd: Command code
  * @param  buf: Buffer containing command data (request parameters)
  * @param  len: Number of data to be sent (in bytes)
  * @retval result (USBD_OK in all cases)
  */
static uint16_t VCP_Ctrl(uint32_t cmd, uint8_t *buf, uint32_t len)
{
	(void) len;
	switch (cmd) {
		case SEND_ENCAPSULATED_COMMAND:
			/* Not  needed for this driver */
			break;
		case GET_ENCAPSULATED_RESPONSE:
			/* Not  needed for this driver */
			break;
		case SET_COMM_FEATURE:
			/* Not  needed for this driver */
			break;
		case GET_COMM_FEATURE:
			/* Not  needed for this driver */
			break;
		case CLEAR_COMM_FEATURE:
			/* Not  needed for this driver */
			break;
		case SET_LINE_CODING:
			linecoding.bitrate = (uint32_t)(buf[0] | (buf[1] << 8)
				| (buf[2] << 16) | (buf[3] << 24));
			linecoding.format = buf[4];
			linecoding.paritytype = buf[5];
			linecoding.datatype = buf[6];
			break;
		case GET_LINE_CODING:
			buf[0] = (uint8_t) (linecoding.bitrate);
			buf[1] = (uint8_t) (linecoding.bitrate >> 8);
			buf[2] = (uint8_t) (linecoding.bitrate >> 16);
			buf[3] = (uint8_t) (linecoding.bitrate >> 24);
			buf[4] = linecoding.format;
			buf[5] = linecoding.paritytype;
			buf[6] = linecoding.datatype;
			break;
		case SET_CONTROL_LINE_STATE:
			/* Not  needed for this driver */
			break;
		case SEND_BREAK:
			/* Not  needed for this driver */
			break;
		default:
			break;
	}

	return USBD_OK;
}

/**
  * @brief  VCP_DataTx
  * @param  buf: Buffer of data to be sent
  * @param  max: Size of buffer (in bytes)
  * @param  len: Returns the size of data to be sent (in bytes)
  * @param  has_more: Returns whether there are more elements in the queue
  * @retval result: USBD_OK if all operations are OK else VCP_FAIL
  */
static uint16_t VCP_DataTx(
	uint8_t *buf, uint32_t max, uint32_t *len, uint8_t *has_more)
{
	/* This function is called from an ISR */
	uint32_t count;

	for (count = 0; count < max; ++count, ++buf) {
		if (xQueueReceiveFromISR(
			os_tx_queue,
			(void *)buf, &xHigherPriorityTaskWoken) == pdFALSE)
				break;
	}
	*len = count;
	*has_more = (xQueueIsQueueEmptyFromISR(os_tx_queue) == pdFALSE);
	return USBD_OK;
}

/**
  * @brief  VCP_DataRx
  *
  *     @note
  *     This function will block any OUT packet reception on USB endpoint
  *     until exiting this function. If you exit this function before transfer
  *     is complete on CDC interface (ie. using DMA controller) it will result
  *     in receiving more data while previous ones are still not sent.
  *
  * @param  buf: Buffer of data received
  * @param  len: Number of data received (in bytes)
  * @retval result: USBD_OK if all operations are OK else VCP_FAIL
  */
static uint16_t VCP_DataRx(uint8_t *buf, uint32_t len)
{
	/* This function is called from an ISR */
	while (len--)
		xQueueSendFromISR(
			os_rx_queue,
			buf++, &xHigherPriorityTaskWoken);
	return USBD_OK;
}

/* Public functions ----------------------------------------------------------*/
void USB_VCP_Init(void *rx_queue, void *tx_queue)
{
	os_rx_queue = rx_queue;
	os_tx_queue = tx_queue;
	USBD_Init(
		&USB_OTG_dev,
#ifdef USE_USB_OTG_HS
		USB_OTG_HS_CORE_ID,
#else /* USE_USB_OTG_HS */
		USB_OTG_FS_CORE_ID,
#endif /* USE_USB_OTG_HS */
		&USR_desc,
		&USBD_CDC_cb,
		&USR_cb);
}

uint8_t USB_VCP_Connected(void)
{
	/* See usbd_core.c:495 */
	return USB_OTG_dev.dev.connection_status;
}

/* IRQ handlers --------------------------------------------------------------*/
#ifdef USE_USB_OTG_HS
void OTG_HS_WKUP_IRQHandler(void)
{
	if (USB_OTG_dev.cfg.low_power)
	{
		*(uint32_t *)(0xE000ED10) &= 0xFFFFFFF9;
		/* SystemInit(); */
		USB_OTG_UngateClock(&USB_OTG_dev);
	}
	EXTI_ClearITPendingBit(EXTI_Line20);
}
#else /* USE_USB_OTG_HS */
void OTG_FS_WKUP_IRQHandler(void)
{
	if (USB_OTG_dev.cfg.low_power)
	{
		*(uint32_t *)(0xE000ED10) &= 0xFFFFFFF9;
		/* SystemInit(); */
		USB_OTG_UngateClock(&USB_OTG_dev);
	}
	EXTI_ClearITPendingBit(EXTI_Line18);
}
#endif /* USE_USB_OTG_HS */

#ifdef USE_USB_OTG_HS
void OTG_HS_IRQHandler(void)
#else /* USE_USB_OTG_HS */
void OTG_FS_IRQHandler(void)
#endif /* USE_USB_OTG_HS */
{
	xHigherPriorityTaskWoken = pdFALSE;
	USBD_OTG_ISR_Handler(&USB_OTG_dev);
	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

#ifdef USB_OTG_HS_DEDICATED_EP1_ENABLED
/**
  * @brief  This function handles EP1_IN Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_IN_IRQHandler(void)
{
	USBD_OTG_EP1IN_ISR_Handler(&USB_OTG_dev);
}

/**
  * @brief  This function handles EP1_OUT Handler.
  * @param  None
  * @retval None
  */
void OTG_HS_EP1_OUT_IRQHandler(void)
{
	USBD_OTG_EP1OUT_ISR_Handler(&USB_OTG_dev);
}
#endif

/* User "hooks" --------------------------------------------------------------*/
static void USBD_USR_Init(void)
{
}

static void USBD_USR_DeviceReset(uint8_t speed)
{
	(void)speed;
}

static void USBD_USR_DeviceConfigured(void)
{
}

static void USBD_USR_DeviceSuspended(void)
{
}

static void USBD_USR_DeviceResumed(void)
{
}

static void USBD_USR_DeviceConnected(void)
{
	/* Clear low-level USB FIFOs to prevent I/O corruption */
	USB_OTG_FlushTxFifo(&USB_OTG_dev, 0x10);
	USB_OTG_FlushRxFifo(&USB_OTG_dev);
}

static void USBD_USR_DeviceDisconnected(void)
{
}

USBD_Usr_cb_TypeDef USR_cb =
{
	USBD_USR_Init,
	USBD_USR_DeviceReset,
	USBD_USR_DeviceConfigured,
	USBD_USR_DeviceSuspended,
	USBD_USR_DeviceResumed,
	USBD_USR_DeviceConnected,
	USBD_USR_DeviceDisconnected,
};

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

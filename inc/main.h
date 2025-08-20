#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

	#include "stm32f10x.h"
	#include <stdint.h>
	#include "goods.h"


	#define MAIN_UART               USART1
	#define MAIN_UART_CLK_EN        BIT_BAND_PERM(RCC->APB2ENR, RCC_APB2ENR_USART1EN) = 1
	//#define MAIN_UART_CLK_EN        RCC->APB2ENR |= RCC_APB2ENR_USART1EN
	#define MAIN_UART_CLK_DIV       1

	#define MAIN_UART_IRQHandler    USART1_IRQHandler
	#define MAIN_UART_IRQn          USART1_IRQn

	#define MAIN_UART_DMA_RX_CH_I     5
	#define MAIN_UART_DMA_RX_CH       DMA1_Channel5
	//#define MAIN_UART_DMA
	//#define MAIN_UART_DMA



#ifdef __cplusplus
}
#endif

#endif /* MAIN_H_INCLUDED */

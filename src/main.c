/*
	~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

	Проверяем, проходят ли прерывания от UART, если принятые данные забираются по DMA.

	Для этого настраиваем:
		1. UART1 на 1 мбит/с, выход TX перемычкой заворачиваем на вход RX (PA9<->PA10). Разрешаем прерывания RX.
		2. DMA, что бы забирал принятое UART.
		3. TIM3, что бы прерывал периодически.
	В обработчиках прерываний от:
		1. TIM3  - отправляем в UART байт, не выходим какое-то время (что бы байт был принят обратно).
		2. UART1 - считаем статистику, сколько раз мы там оказались, и сколько байт успели принять по DMA
		   (если вообще в этом обработчике оказались)

	Делаем как минимум дважды - с разными приоритетами прерываний (TIM приоритетнее UART и наоборот).
	В обработчиках прерываний дёргаем пинами, что бы смотреть на осциллоскопе.

	Приоритеты прерываний от USART1 и TIM3 задаются соответственно макроопределениями
	UART_IRQ_PRIORITY и TIM_IRQ_PRIORITY. Чем ниже значение, тем выше приоритет.

	~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

	Результаты:

	1. Независимо от приоритета, прерывание USART RXNE вызывается в любом случае.
	2. Флаг RXNE в SR USART при этом всегда равен 0, т.к. буфер приёмника опустошается аппаратно через DMA
	   (то есть, наверняка выяснить причину прерывания уже не получится).

	~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~-~

*/


#include "main.h"
#include "pins.h"
#include "bitbanding.h"

#define MAIN_UART_BAUD 1000000
#define UART_IRQ_PRIORITY 4 //2
#define TIM_IRQ_PRIORITY 3


uint8_t           uart_rx_dma_buff[256];      // сюда по DMA попадают принятые UART данные
uint32_t          last_dmarx_CNDTR;           // последнее считанное значение DMA->CNDTR
volatile uint32_t UART_IRQ_cnt = 0;           // счётчик прерываний UART1
volatile uint32_t TIM_IRQ_cnt = 0;            // счётчик прерываний TIM3
int               rx_routine_stat[8] = {0};   // статистика - сколько байт готово в uart_rx_dma_buff при очередном вызове Uart_RxRoutine
int               rx_ne_stat[2] = {0};        // статистика - значение флвга RXNE в USART1->SR в обработчике прерываний USART1_IRQHandler
volatile int      rx_count;                   // счётчик принятых байт в Uart_RxRoutine

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Инициализируем UART1 на приём/передачу.
// Принятыйё байты будем складывать в uart_rx_dma_buff
// при помощи DMA.
void init_uart(void)
{

	MAIN_UART_CLK_EN;
	int clk_freq = SystemCoreClock / MAIN_UART_CLK_DIV;

	BIT_BAND_PERM(RCC->AHBENR, RCC_AHBENR_DMA1EN) = 1;

	// инициализируем DMA забирать принятые по UART данные и бесконечно складывать их
	// в буфер uart_rx_dma_buff. Время от времени будем вызывать рутину и проверять
	// указатель DMA->CNDTR, и при необходимости отдавать принятое

	//DMA1->IFCR = 0x0000000f << ((MAIN_UART_DMA_RX_CH_I - 1) * 4);  // Reset interrupt flag for 1  channel
	MAIN_UART_DMA_RX_CH->CCR   = 0;
	MAIN_UART_DMA_RX_CH->CMAR  = (uint32_t)uart_rx_dma_buff;
	MAIN_UART_DMA_RX_CH->CPAR  = (uint32_t)&MAIN_UART->DR;
	MAIN_UART_DMA_RX_CH->CNDTR = last_dmarx_CNDTR = NUMOFARRAY(uart_rx_dma_buff);
	MAIN_UART_DMA_RX_CH->CCR   = DMA_CCR1_EN | DMA_CCR1_MINC | DMA_CCR1_CIRC;

	MAIN_UART->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
	MAIN_UART->BRR = clk_freq / MAIN_UART_BAUD;
	MAIN_UART->CR3 = USART_CR3_DMAR;

	NVIC_SetPriority(MAIN_UART_IRQn, UART_IRQ_PRIORITY);
	NVIC_EnableIRQ(MAIN_UART_IRQn); // enable USART interrupt

}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Инициализируем TIM3 как источник прерываний
void init_tim(void)
{
	BIT_BAND_PERM(RCC->APB1ENR, RCC_APB1ENR_TIM3EN) = 1;

	//NVIC_SetPriority(DMA1_Channel3_IRQn, 2);  - ?
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, TIM_IRQ_PRIORITY);


	TIM3->ARR  = SystemCoreClock / 10000 - 1;   //
	TIM3->DIER = TIM_DIER_UIE;
	// запускаем таймер
	TIM3->CR1  = TIM_CR1_CEN;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void TIM3_IRQHandler(void)
{
	GPIO_OUT_VAL(PIN_STM_DIAG1, 1);

	TIM3->SR = 0;  // clear interrupt flags

	TIM_IRQ_cnt++;

	MAIN_UART->DR = 'U'; // стартуем передачу байта

	// ждём 3/4 цикла счётчика
	int wait_to = TIM3->ARR;
	wait_to -= wait_to / 4;
	while(TIM3->CNT < wait_to);


	GPIO_OUT_VAL(PIN_STM_DIAG1, 0);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void MAIN_UART_IRQHandler(void)
{
	GPIO_OUT_VAL(PIN_STM_DIAG2, 1);
	rx_ne_stat[!!(MAIN_UART->SR & USART_SR_RXNE)]++;
	UART_IRQ_cnt++;
	GPIO_OUT_VAL(PIN_STM_DIAG2, 0);
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Функция, где мы что-то делаем с принятыми по UART данными.
// (Сейчас только считаем общий объём)
// Параметры:
// src - массив с данными
// len - кол-во байт в src
//
//
void rx_buff(char *src, int len)
{
	rx_count += len;  // сколько всего принято
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Смотрим указатель DMA CNDTR, при необходимости
// отдаём принятое.
// время от времени вызывается из main.
void Uart_RxRoutine(void)
{
	uint32_t cndtr = MAIN_UART_DMA_RX_CH->CNDTR;

	if(cndtr != last_dmarx_CNDTR)
	{
		GPIO_OUT_VAL(PIN_STM_DIAG3, 1);
		uint32_t bsize  = NUMOFARRAY(uart_rx_dma_buff);
		uint32_t remain = (last_dmarx_CNDTR - cndtr) % bsize; // сколько успели принять
		uint32_t tail   = bsize - last_dmarx_CNDTR;  // указатель на хвост (откуда читать)

		rx_routine_stat[min(NUMOFARRAY(rx_routine_stat) - 1, remain)]++;  // собираем статистику - сколько байт успели накопить с момента предыдущего вызова

		while(remain)
		{
			uint32_t to_copy = min(remain, bsize - tail);     // размер непрерывного куска данных
			rx_buff((char*)uart_rx_dma_buff + tail, to_copy); // вдумчиво обрабатываем принятые данные

			tail += to_copy;
			if(tail >= bsize) tail -= bsize;
			remain -= to_copy;
		}

		last_dmarx_CNDTR = cndtr;

		GPIO_OUT_VAL(PIN_STM_DIAG3, 0);
	}

}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

int main(void)
{
	init_all_gpios();
	init_uart();
	init_tim();

    while(1)
    {
		Uart_RxRoutine();
		__WFE();
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



#include "goods.h"
#include "pins.h"

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

struct s_PinDef g_PinsUart1[] =
{
	{GPIOA,  9,  GPIO_Speed_10MHz | GPIO_Mode_AF_PP,     1},  // UART1_TX
	{GPIOA, 10,  GPIO_Speed_input | GPIO_Mode_IN_FLOAT,  1},  // UART1_RX
};

struct s_PinDef g_PinsDiag[] =
{
	{GPIOB,  7,  GPIO_Speed_10MHz | GPIO_Mode_OUT_PP,  1},  // 0 - STM_DIAG1
	{GPIOB,  6,  GPIO_Speed_10MHz | GPIO_Mode_OUT_PP,  1},  // 1 - STM_DIAG2
	{GPIOB,  5,  GPIO_Speed_10MHz | GPIO_Mode_OUT_PP,  1},  // 2 - STM_DIAG3
	{GPIOB,  4,  GPIO_Speed_10MHz | GPIO_Mode_OUT_PP,  1},  // 3 - STM_DIAG4
};

// all pins
const struct s_PinDefSet g_AllPins[] =
{
	GPIO_ARRAY(g_PinsUart1    ), // 0
	GPIO_ARRAY(g_PinsDiag     ), // 1
};




//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// проверяем корректность описания выводов и что
// бы один вывод не был использован дважды.
// Достаточно после составления списка выводов
// вызвать 1 раз в режиме отладки.
// В релизе вызывать не надо
void pins_check(void)
{
	void *pgpio[] =
	{
		GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
	};
	uint16_t used[] = {0,0,0,0,0,0,0,0,0,0,0,0};


    for(int in_all = 0; in_all < NUMOFARRAY(g_AllPins); in_all++)
    {
        const struct s_PinDefSet *set = g_AllPins + in_all;
        for(int idx = 0; idx < set->num; idx++)
        {
            const struct s_PinDef *p = set->pindef + idx;

            // ищем GPIO
            int gpio_idx;
            for(gpio_idx = 0; gpio_idx < NUMOFARRAY(pgpio); gpio_idx++)
			{
				if(p->port == pgpio[gpio_idx]) break;
			}

			if(gpio_idx >= NUMOFARRAY(pgpio))
			{
				for(;;);  // ошибка - неправильный порт GPIO (нет в массове pgpio)
			}

			uint16_t mask = 1 << p->pin;
			if(!mask)
			{
				for(;;);  // ошибка - неправильный номер ножки (выходит за пределы 0..15)
			}

			if(used[gpio_idx] & mask)
			{
				for(;;);  // ошибка - ножка уже использована ранее ('in_all' - номер в массиве g_AllPins, 'idx' - номер в соотв. подмассиве)
			}

			used[gpio_idx] |= mask;
        }
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// инициализируем все ножки
// вызвать 1 раз из main
void init_all_gpios(void)
{
	//pins_check();

	#if USB_CONNECT_PIN
		RCC->APB2ENR |= RCC_APB2ENR_AFIOEN;
		AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_1;        // disable fckng JTAG - make A15 as GPIO
	#endif // USB_CONNECT_PIN

	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPDEN;

    for(int a = 0; a < NUMOFARRAY(g_AllPins); a++)
    {
        const struct s_PinDefSet *set = g_AllPins + a;
        for(int i = 0; i < set->num; i++)
        {
            const struct s_PinDef *p = set->pindef + i;

            int bit_pos = p->pin + (p->init_val ? 0 : 16);
            p->port->BSRR = 1 << bit_pos;
            GPIO_SET_MODEp(p, p->mod);
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
uint32_t pins_read(const struct s_PinDef *pins[], int pins_num)
{
	uint32_t val = 0;
	// читаем значения с ножек
	for(int i = pins_num - 1; i >= 0; i--)
	{
		val <<= 1;
		val |= GPIO_GET_PINp(pins[i]);
	}
	return val;
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void pins_write(const struct s_PinDef *pins[], int pins_num, uint32_t val)
{
	for(int i = 0; i < pins_num; i++)
	{
		GPIO_OUT_VALp(pins[i], val & 1);
		val >>= 1;
	}
}
//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

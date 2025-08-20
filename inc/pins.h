#ifndef PINS_H_INCLUDED
#define PINS_H_INCLUDED

	#include "gpio_F1xx.h"

	void init_all_gpios(void);
	uint32_t pins_read(const struct s_PinDef *pins[], int pins_num);
	void pins_write(const struct s_PinDef *pins[], int pins_num, uint32_t val);

	extern struct s_PinDef g_PinsDiag[];
	#define PIN_STM_DIAG1        g_PinsDiag[0]
	#define PIN_STM_DIAG2        g_PinsDiag[1]
	#define PIN_STM_DIAG3        g_PinsDiag[2]
	#define PIN_STM_DIAG4        g_PinsDiag[3]


#endif /* PINS_H_INCLUDED */

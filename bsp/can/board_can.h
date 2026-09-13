#ifndef BOARD_CAN_H_
#define BOARD_CAN_H_

/* Unlock and disable watchdog timer */
void disable_WDOG(void);

/* Initialize external 8 MHz oscillator, CAN pin routing, and board peripherals */
void init_MCU(void);


#endif /* BOARD_CAN_H_ */
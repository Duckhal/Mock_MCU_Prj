#ifndef BOARD_H_
#define BOARD_H_

#include "../drivers/common/Driver_Common.h"

/* Push Buttons (Active-Low with Pull-Up on S32K144 EVB) */
#define BUTTON_SW2              ARM_GPIO_PIN(PORT_C_IDX, 12U)   /* PTC12 */
#define BUTTON_SW3              ARM_GPIO_PIN(PORT_C_IDX, 13U)   /* PTC13 */

/* RGB LED (Active-Low on S32K144 EVB) */
#define LED_BLUE                ARM_GPIO_PIN(PORT_D_IDX, 0U)    /* PTD0  */
#define LED_RED                 ARM_GPIO_PIN(PORT_D_IDX, 15U)   /* PTD15 */
#define LED_GREEN               ARM_GPIO_PIN(PORT_D_IDX, 16U)   /* PTD16 */

/* Logic Level Definitions */
#define LED_ON                  0U
#define LED_OFF                 1U

#endif /* BOARD_H_ */

#ifndef BSP_LED_H_
#define BSP_LED_H_

#include "../drivers/gpio/Driver_GPIO.h"
#include <stdint.h>

/* Hardware Pin Definitions for S32K144 EVB RGB LED */
#ifndef LED_BLUE
#define LED_BLUE    GPIO_D0   /* PTD0:  Blue LED  */
#endif

#ifndef LED_RED
#define LED_RED     GPIO_D15  /* PTD15: Red LED   */
#endif

#ifndef LED_GREEN
#define LED_GREEN   GPIO_D16  /* PTD16: Green LED */
#endif

/* State definition matching active-low logic on S32K144 EVB */
typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON  = 1
} led_state_t;

/**
 * @brief Initialize pin as push-pull output and turn off LED by default.
 * @param pin Pin identifier (e.g. LED_GREEN, GPIO_D16).
 */
void LED_Init(ARM_GPIO_Pin_t pin);

/**
 * @brief Write explicit state to LED (LED_STATE_ON or LED_STATE_OFF).
 * @param pin Pin identifier.
 * @param state Desired LED state.
 */
void LED_Write(ARM_GPIO_Pin_t pin, led_state_t state);

/**
 * @brief Turn ON the specified LED.
 * @param pin Pin identifier.
 */
void LED_On(ARM_GPIO_Pin_t pin);

/**
 * @brief Turn OFF the specified LED.
 * @param pin Pin identifier.
 */
void LED_Off(ARM_GPIO_Pin_t pin);

/**
 * @brief Toggle the current output state of the specified LED.
 * @param pin Pin identifier.
 */
void LED_Toggle(ARM_GPIO_Pin_t pin);

#endif /* BSP_LED_H_ */

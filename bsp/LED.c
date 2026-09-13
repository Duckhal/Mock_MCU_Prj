#include "led.h"

/* Active-Low: 0 = ON, 1 = OFF */
#define LED_PIN_LEVEL_ON   (0U)
#define LED_PIN_LEVEL_OFF  (1U)

void LED_Init(ARM_GPIO_Pin_t pin)
{
    /* Configure pin muxing and enable peripheral port clock */
    Driver_GPIO0.Setup(pin, NULL);

    /* Set pin as output with push-pull mode */
    Driver_GPIO0.SetDirection(pin, ARM_GPIO_OUTPUT);
    Driver_GPIO0.SetOutputMode(pin, ARM_GPIO_PUSH_PULL);

    /* Turn off LED by default */
    Driver_GPIO0.SetOutput(pin, LED_PIN_LEVEL_OFF);
}

void LED_Write(ARM_GPIO_Pin_t pin, led_state_t state)
{
    uint32_t val = (state == LED_STATE_ON) ? LED_PIN_LEVEL_ON : LED_PIN_LEVEL_OFF;
    Driver_GPIO0.SetOutput(pin, val);
}

void LED_On(ARM_GPIO_Pin_t pin)
{
    Driver_GPIO0.SetOutput(pin, LED_PIN_LEVEL_ON);
}

void LED_Off(ARM_GPIO_Pin_t pin)
{
    Driver_GPIO0.SetOutput(pin, LED_PIN_LEVEL_OFF);
}

void LED_Toggle(ARM_GPIO_Pin_t pin)
{
    /* Read current output latch and flip logic level */
    uint32_t current_level = Driver_GPIO0.GetInput(pin);
    Driver_GPIO0.SetOutput(pin, current_level ^ 1U);
}
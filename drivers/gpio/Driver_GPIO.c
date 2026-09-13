#include "Driver_GPIO.h"
#include "../nvic/Driver_NVIC.h"
#include "S32K144.h"


#define GPIO_MAX_PINS 160U
#define PIN_IS_AVAILABLE(n) ((n) < GPIO_MAX_PINS)

typedef struct
{
    GPIO_Type* gpio;
    PORT_Type* port;
    IRQn_Type irq;
    uint32_t pcc_index;
} S32K144_GPIO_Port;

static const S32K144_GPIO_Port gpio_ports[5] = {
    {PTA, PORTA, PORTA_IRQn, PCC_PORTA_INDEX},
    {PTB, PORTB, PORTB_IRQn, PCC_PORTB_INDEX},
    {PTC, PORTC, PORTC_IRQn, PCC_PORTC_INDEX},
    {PTD, PORTD, PORTD_IRQn, PCC_PORTD_INDEX},
    {PTE, PORTE, PORTE_IRQn, PCC_PORTE_INDEX},
};

/* Store Callbacks */
static ARM_GPIO_SignalEvent_t callbacks[GPIO_MAX_PINS];
static ARM_GPIO_EVENT_TRIGGER triggers[GPIO_MAX_PINS];

static int32_t pin_decoder(ARM_GPIO_Pin_t pin, uint32_t* port_index, uint32_t* bit)
{
    if (pin >= GPIO_MAX_PINS)
    {
        return ARM_GPIO_ERROR_PIN;
    }
    *port_index = pin / 32;
    *bit = pin % 32;
    return ARM_DRIVER_OK;
}
/* Enable Clock for Port */
static void enable_port_clock(uint32_t port_index)
{
    PCC->PCCn[gpio_ports[port_index].pcc_index] |= PCC_PCCn_CGC_MASK;
}

/* Enable Interrupt for Port */
static void enable_irq(IRQn_Type irq)
{
    NVIC_EnableIRQ(irq);
}

/* Enable Interrupt for Port */
static int32_t GPIO_Setup(ARM_GPIO_Pin_t pin, ARM_GPIO_SignalEvent_t cb_event)
{
    uint32_t port_index, bit, pcr;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
    {
        return ARM_GPIO_ERROR_PIN;
    }
    enable_port_clock(port_index);
    pcr = gpio_ports[port_index].port->PCR[bit];
    pcr &= ~(PORT_PCR_MUX_MASK | PORT_PCR_PE_MASK |
        PORT_PCR_PS_MASK | PORT_PCR_IRQC_MASK);

    gpio_ports[port_index].port->PCR[bit] = pcr | PORT_PCR_MUX(1);
    gpio_ports[port_index].gpio->PDDR &= ~(1UL << bit);
    callbacks[pin] = cb_event;
    triggers[pin] = ARM_GPIO_TRIGGER_NONE;

    return ARM_DRIVER_OK;
}

/* Set Direction */
static int32_t GPIO_SetDirection(ARM_GPIO_Pin_t pin, ARM_GPIO_DIRECTION direction)
{
    uint32_t port_index, bit;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
    {
        return ARM_GPIO_ERROR_PIN;
    }
    if (direction == ARM_GPIO_OUTPUT)
    {
        /* Output */
        gpio_ports[port_index].gpio->PDDR |= (1UL << bit);
    }
    else if (direction == ARM_GPIO_INPUT)
    {
        gpio_ports[port_index].gpio->PDDR &= ~(1UL << bit);
    }
    else
    {
        return ARM_DRIVER_ERROR_PARAMETER;
    }
    return ARM_DRIVER_OK;
}

/* Output Mode */
static int32_t GPIO_SetOutputMode(ARM_GPIO_Pin_t pin, ARM_GPIO_OUTPUT_MODE mode)
{
    uint32_t port_index, bit;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
        return ARM_GPIO_ERROR_PIN;
    /* The S32K144 PORT PCR does not provide an open-drain control bit. */
    if (mode == ARM_GPIO_PUSH_PULL)
        return ARM_DRIVER_OK;
    if (mode == ARM_GPIO_OPEN_DRAIN)
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    return ARM_DRIVER_ERROR_PARAMETER;
}

/* Set Resistor */
static int32_t GPIO_SetPullResistor(ARM_GPIO_Pin_t pin, ARM_GPIO_PULL_RESISTOR resistor)
{
    uint32_t port_index, bit, pcr;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
        return ARM_GPIO_ERROR_PIN;
    pcr = gpio_ports[port_index].port->PCR[bit] & ~(PORT_PCR_PE_MASK | PORT_PCR_PS_MASK);
    if (resistor == ARM_GPIO_PULL_UP)
        pcr |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    else if (resistor == ARM_GPIO_PULL_DOWN)
        pcr |= PORT_PCR_PE_MASK;
    else if (resistor != ARM_GPIO_PULL_NONE)
        return ARM_DRIVER_ERROR_PARAMETER;
    gpio_ports[port_index].port->PCR[bit] = pcr;
    return ARM_DRIVER_OK;
}

/* Set Trigger */
static int32_t GPIO_SetEventTrigger(ARM_GPIO_Pin_t pin, ARM_GPIO_EVENT_TRIGGER trigger)
{
    uint32_t port_index, bit, irqc, pcr;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
    {
        return ARM_GPIO_ERROR_PIN;
    }
    switch (trigger)
    {
    case ARM_GPIO_TRIGGER_NONE:
        irqc = 0x0U;
        break;
    case ARM_GPIO_TRIGGER_RISING_EDGE:
        irqc = 0x9U;
        break;
    case ARM_GPIO_TRIGGER_FALLING_EDGE:
        irqc = 0xAU;
        break;
    case ARM_GPIO_TRIGGER_EITHER_EDGE:
        irqc = 0xBU;
        break;
    default:
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    pcr = gpio_ports[port_index].port->PCR[bit] & ~PORT_PCR_IRQC_MASK;
    gpio_ports[port_index].port->PCR[bit] = pcr | PORT_PCR_IRQC(irqc);
    triggers[pin] = trigger;

    if (trigger != ARM_GPIO_TRIGGER_NONE)
        enable_irq(gpio_ports[port_index].irq);

    return ARM_DRIVER_OK;
}

/* Set Output */
static void GPIO_SetOutput(ARM_GPIO_Pin_t pin, uint32_t val)
{
    uint32_t port_index, bit;
    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
    {
        return;
    }
    if (val != 0U)
    {
        gpio_ports[port_index].gpio->PSOR = (1UL << bit);
    }
    else
    {
        gpio_ports[port_index].gpio->PCOR = (1UL << bit);
    }
}

/* Set Input */
static uint32_t GPIO_GetInput(ARM_GPIO_Pin_t pin)
{
    uint32_t port_index, bit;

    if (pin_decoder(pin, &port_index, &bit) != ARM_DRIVER_OK)
        return 0U;

    return (gpio_ports[port_index].gpio->PDIR >> bit) & 1U;
}

/* Event */
static void GPIO_PortIRQHandler(uint32_t port_index)
{
    uint32_t flags = gpio_ports[port_index].port->ISFR;
    gpio_ports[port_index].port->ISFR = flags; /* Clear flag */

    for (uint32_t bit = 0; bit < 32; bit++)
    {
        if ((flags & (1UL << bit)) != 0U)
        {
            ARM_GPIO_Pin_t pin = port_index * 32 + bit;
            if (callbacks[pin] != 0)
            {
                uint32_t event = (triggers[pin] == ARM_GPIO_TRIGGER_RISING_EDGE)
                    ? ARM_GPIO_EVENT_RISING_EDGE
                    : (triggers[pin] == ARM_GPIO_TRIGGER_FALLING_EDGE)
                    ? ARM_GPIO_EVENT_FALLING_EDGE
                    : ARM_GPIO_EVENT_EITHER_EDGE;
                callbacks[pin](pin, event);
            }
        }
    }
}

void PORTA_IRQHandler(void) { GPIO_PortIRQHandler(0U); }
void PORTB_IRQHandler(void) { GPIO_PortIRQHandler(1U); }
void PORTC_IRQHandler(void) { GPIO_PortIRQHandler(2U); }
void PORTD_IRQHandler(void) { GPIO_PortIRQHandler(3U); }
void PORTE_IRQHandler(void) { GPIO_PortIRQHandler(4U); }

ARM_DRIVER_GPIO Driver_GPIO0 = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput };

#ifndef DRIVER_GPIO_H_
#define DRIVER_GPIO_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "../common/Driver_Common.h"
#include <stdint.h>

  /**
  \brief GPIO Pin
  */
  typedef uint32_t ARM_GPIO_Pin_t;
/* GPIO Event Types */
#define ARM_GPIO_EVENT_RISING_EDGE (1U)
#define ARM_GPIO_EVENT_FALLING_EDGE (2U)
#define ARM_GPIO_EVENT_EITHER_EDGE (3U)
  /**
  \brief GPIO Direction
  */
  typedef enum
  {
    ARM_GPIO_INPUT = 0, ///< Input (default)
    ARM_GPIO_OUTPUT     ///< Output
  } ARM_GPIO_DIRECTION;

  typedef enum
  {
    ARM_GPIO_PUSH_PULL = 0, ///< Push-pull (default)
    ARM_GPIO_OPEN_DRAIN
  } ARM_GPIO_OUTPUT_MODE;
  /**
  \brief GPIO Pull Resistor
  */
  typedef enum
  {
    ARM_GPIO_PULL_NONE = 0, ///< None (default)
    ARM_GPIO_PULL_UP,       ///< Pull-up
    ARM_GPIO_PULL_DOWN      ///< Pull-down
  } ARM_GPIO_PULL_RESISTOR;
  /**
  \brief GPIO Event Trigger
  */
  typedef enum
  {
    ARM_GPIO_TRIGGER_NONE = 0,     ///< None (default)
    ARM_GPIO_TRIGGER_RISING_EDGE,  ///< Rising-edge
    ARM_GPIO_TRIGGER_FALLING_EDGE, ///< Falling-edge
    ARM_GPIO_TRIGGER_EITHER_EDGE   ///< Either edge (rising and falling)
  } ARM_GPIO_EVENT_TRIGGER;

#define ARM_GPIO_ERROR_PIN (ARM_DRIVER_ERROR_SPECIFIC - 1) ///< Specified Pin not available
  // Function documentation
  /**
    \fn          int32_t ARM_GPIO_Setup (ARM_GPIO_Pin_t pin, ARM_GPIO_SignalEvent_t cb_event)
    \brief       Setup GPIO Interface.
    \param[in]   pin  GPIO Pin
    \param[in]   cb_event  Pointer to \ref ARM_GPIO_SignalEvent
    \return      \ref execution_status

    \fn          int32_t ARM_GPIO_SetDirection (ARM_GPIO_Pin_t pin, ARM_GPIO_DIRECTION direction)
    \brief       Set GPIO Direction.
    \param[in]   pin  GPIO Pin
    \param[in]   direction  \ref ARM_GPIO_DIRECTION
    \return      \ref execution_status

    \fn          int32_t ARM_GPIO_SetOutputMode (ARM_GPIO_Pin_t pin, ARM_GPIO_OUTPUT_MODE mode)
    \brief       Set GPIO Output Mode.
    \param[in]   pin  GPIO Pin
    \param[in]   mode  \ref ARM_GPIO_OUTPUT_MODE
    \return      \ref execution_status

    \fn          int32_t ARM_GPIO_SetPullResistor (ARM_GPIO_Pin_t pin, ARM_GPIO_PULL_RESISTOR resistor)
    \brief       Set GPIO Pull Resistor.
    \param[in]   pin  GPIO Pin
    \param[in]   resistor  \ref ARM_GPIO_PULL_RESISTOR
    \return      \ref execution_status

    \fn          int32_t ARM_GPIO_SetEventTrigger (ARM_GPIO_Pin_t pin, ARM_GPIO_EVENT_TRIGGER trigger)
    \brief       Set GPIO Event Trigger.
    \param[in]   pin  GPIO Pin
    \param[in]   trigger  \ref ARM_GPIO_EVENT_TRIGGER
    \return      \ref execution_status

    \fn          void ARM_GPIO_SetOutput (ARM_GPIO_Pin_t pin, uint32_t val)
    \brief       Set GPIO Output Level.
    \param[in]   pin  GPIO Pin
    \param[in]   val  GPIO Pin Level (0 or 1)

    \fn          uint32_t ARM_GPIO_GetInput (ARM_GPIO_Pin_t pin)
    \brief       Get GPIO Input Level.
    \param[in]   pin  GPIO Pin
    \return      GPIO Pin Level (0 or 1)

    \fn          void ARM_GPIO_SignalEvent (ARM_GPIO_Pin_t pin, uint32_t event)
    \brief       Signal GPIO Events.
    \param[in]   pin    GPIO Pin on which event occurred
    \param[in]   event  \ref GPIO_events notification mask
  */

  typedef void (*ARM_GPIO_SignalEvent_t)(ARM_GPIO_Pin_t pin, uint32_t event); /* Pointer to \ref ARM_GPIO_SignalEvent : Signal GPIO Event */

  typedef struct _ARM_DRIVER_GPIO
  {
    int32_t (*Setup)(ARM_GPIO_Pin_t pin, ARM_GPIO_SignalEvent_t cb_event);           ///< Pointer to \ref ARM_GPIO_Setup : Setup GPIO Interface.
    int32_t (*SetDirection)(ARM_GPIO_Pin_t pin, ARM_GPIO_DIRECTION direction);       ///< Pointer to \ref ARM_GPIO_SetDirection : Set GPIO Direction.
    int32_t (*SetOutputMode)(ARM_GPIO_Pin_t pin, ARM_GPIO_OUTPUT_MODE mode);         ///< Pointer to \ref ARM_GPIO_SetOutputMode : Set GPIO Output Mode.
    int32_t (*SetPullResistor)(ARM_GPIO_Pin_t pin, ARM_GPIO_PULL_RESISTOR resistor); ///< Pointer to \ref ARM_GPIO_SetPullResistor : Set GPIO Pull Resistor.
    int32_t (*SetEventTrigger)(ARM_GPIO_Pin_t pin, ARM_GPIO_EVENT_TRIGGER trigger);  ///< Pointer to \ref ARM_GPIO_SetEventTrigger : Set GPIO Event Trigger.
    void (*SetOutput)(ARM_GPIO_Pin_t pin, uint32_t val);                             ///< Pointer to \ref ARM_GPIO_SetOutput : Set GPIO Output Level.
    uint32_t (*GetInput)(ARM_GPIO_Pin_t pin);                                        ///< Pointer to \ref ARM_GPIO_GetInput : Get GPIO Input Level.
  } const ARM_DRIVER_GPIO;

#define PORT_A (0)
#define PORT_B (1)
#define PORT_C (2)
#define PORT_D (3)
#define PORT_E (4)
#define PORT_ID(Port, Pin) (((Port) << 5) + (Pin))
extern ARM_DRIVER_GPIO Driver_GPIO0;

  typedef enum
  {
    GPIO_A0 = PORT_ID(PORT_A, 0),
    GPIO_A1,
    GPIO_A2,
    GPIO_A3,
    GPIO_A4,
    GPIO_A5,
    GPIO_A6,
    GPIO_A7,
    GPIO_A8,
    GPIO_A9,
    GPIO_A10,
    GPIO_A11,
    GPIO_A12,
    GPIO_A13,
    GPIO_A14,
    GPIO_A15,
    GPIO_A16,
    GPIO_A17,
    GPIO_A18,
    GPIO_A19,
    GPIO_A20,
    GPIO_A21,
    GPIO_A22,
    GPIO_A23,
    GPIO_A24,
    GPIO_A25,
    GPIO_A26,
    GPIO_A27,
    GPIO_A28,
    GPIO_A29,
    GPIO_A30,
    GPIO_A31,
    GPIO_B0 = PORT_ID(PORT_B, 0),
    GPIO_B1,
    GPIO_B2,
    GPIO_B3,
    GPIO_B4,
    GPIO_B5,
    GPIO_B6,
    GPIO_B7,
    GPIO_B8,
    GPIO_B9,
    GPIO_B10,
    GPIO_B11,
    GPIO_B12,
    GPIO_B13,
    GPIO_B14,
    GPIO_B15,
    GPIO_B16,
    GPIO_B17,
    GPIO_B18,
    GPIO_B19,
    GPIO_B20,
    GPIO_B21,
    GPIO_B22,
    GPIO_B23,
    GPIO_B24,
    GPIO_B25,
    GPIO_B26,
    GPIO_B27,
    GPIO_B28,
    GPIO_B29,
    GPIO_B30,
    GPIO_B31,
    GPIO_C0 = PORT_ID(PORT_C, 0),
    GPIO_C1,
    GPIO_C2,
    GPIO_C3,
    GPIO_C4,
    GPIO_C5,
    GPIO_C6,
    GPIO_C7,
    GPIO_C8,
    GPIO_C9,
    GPIO_C10,
    GPIO_C11,
    GPIO_C12,
    GPIO_C13,
    GPIO_C14,
    GPIO_C15,
    GPIO_C16,
    GPIO_C17,
    GPIO_C18,
    GPIO_C19,
    GPIO_C20,
    GPIO_C21,
    GPIO_C22,
    GPIO_C23,
    GPIO_C24,
    GPIO_C25,
    GPIO_C26,
    GPIO_C27,
    GPIO_C28,
    GPIO_C29,
    GPIO_C30,
    GPIO_C31,
    GPIO_D0 = PORT_ID(PORT_D, 0),
    GPIO_D1,
    GPIO_D2,
    GPIO_D3,
    GPIO_D4,
    GPIO_D5,
    GPIO_D6,
    GPIO_D7,
    GPIO_D8,
    GPIO_D9,
    GPIO_D10,
    GPIO_D11,
    GPIO_D12,
    GPIO_D13,
    GPIO_D14,
    GPIO_D15,
    GPIO_D16,
    GPIO_D17,
    GPIO_D18,
    GPIO_D19,
    GPIO_D20,
    GPIO_D21,
    GPIO_D22,
    GPIO_D23,
    GPIO_D24,
    GPIO_D25,
    GPIO_D26,
    GPIO_D27,
    GPIO_D28,
    GPIO_D29,
    GPIO_D30,
    GPIO_D31,
    GPIO_E0 = PORT_ID(PORT_E, 0),
    GPIO_E1,
    GPIO_E2,
    GPIO_E3,
    GPIO_E4,
    GPIO_E5,
    GPIO_E6,
    GPIO_E7,
    GPIO_E8,
    GPIO_E9,
    GPIO_E10,
    GPIO_E11,
    GPIO_E12,
    GPIO_E13,
    GPIO_E14,
    GPIO_E15,
    GPIO_E16,
    GPIO_E17,
    GPIO_E18,
    GPIO_E19,
    GPIO_E20,
    GPIO_E21,
    GPIO_E22,
    GPIO_E23,
    GPIO_E24,
    GPIO_E25,
    GPIO_E26,
    GPIO_E27,
    GPIO_E28,
    GPIO_E29,
    GPIO_E30,
    GPIO_E31,
  } GPIO_PIN;

#ifdef __cplusplus
}
#endif
#endif /* DRIVER_GPIO_H_*/

#ifndef SYSTEM_CFG_H_
#define SYSTEM_CFG_H_

/* Optional integration fixtures; production builds keep both disabled. */
#ifndef SYSTEM_RUN_CANTP_LOOPBACK_TEST
#define SYSTEM_RUN_CANTP_LOOPBACK_TEST (0U)
#endif

#ifndef SYSTEM_ENABLE_UART_CANTP_LOOPBACK
#define SYSTEM_ENABLE_UART_CANTP_LOOPBACK (0U)
#endif

#endif /* SYSTEM_CFG_H_ */

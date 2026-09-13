#ifndef CAN_LOOPBACK_TEST_H_
#define CAN_LOOPBACK_TEST_H_

typedef enum
{
    CAN_LOOPBACK_TEST_NOT_RUN = 0,
    CAN_LOOPBACK_TEST_RUNNING,
    CAN_LOOPBACK_TEST_PASSED,
    CAN_LOOPBACK_TEST_FAILED_INIT,
    CAN_LOOPBACK_TEST_FAILED_CALLBACKS,
    CAN_LOOPBACK_TEST_FAILED_START,
    CAN_LOOPBACK_TEST_FAILED_WRITE,
    CAN_LOOPBACK_TEST_FAILED_BUS_OFF,
    CAN_LOOPBACK_TEST_FAILED_TIMEOUT,
    CAN_LOOPBACK_TEST_FAILED_DATA
} Can_LoopbackTestResultType;

extern volatile Can_LoopbackTestResultType g_CanLoopbackTestResult;

/** Run one bounded CAN0 internal loopback test and publish its result. */
void Can_LoopbackTest_Run(void);

#endif /* CAN_LOOPBACK_TEST_H_ */

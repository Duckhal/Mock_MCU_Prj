#include "Com.h"
#include "../pdur/PduR.h"
#include <stddef.h>
#include <string.h>

#define COM_MAX_IPDU_LENGTH (8U)

typedef struct
{
    uint16_t counter;
    uint8_t pending;
    uint8_t retryCount;
} Com_TxRuntimeType;

static uint8_t Com_Initialized;
static uint8_t Com_Buffer[COM_NUM_IPDUS][COM_MAX_IPDU_LENGTH];
static uint8_t Com_RxValid[COM_NUM_IPDUS];
static Com_TxRuntimeType Com_TxRuntime[COM_NUM_IPDUS];
volatile uint32_t Com_TxConfirmationCount;
volatile uint32_t Com_RxIndicationCount;
volatile uint32_t Com_TxDropCount;
volatile Com_LogRecordType Com_LogRecords[COM_LOG_CAPACITY];
volatile uint32_t Com_LogSequence;

/** Append a bounded COM event with an I-PDU ID and retry or length detail. */
static void Com_Log(Com_LogEventType event, PduIdType id, uint32_t detail)
{
    uint32_t sequence = Com_LogSequence;
    uint32_t slot = sequence % COM_LOG_CAPACITY;
    Com_LogRecords[slot].event = event;
    Com_LogRecords[slot].ipduId = id;
    Com_LogRecords[slot].detail = detail;
    Com_LogRecords[slot].sequence = sequence;
    Com_LogSequence = sequence + 1U;
}

/** Find exactly one I-PDU with the requested local ID. */
static const Com_IPduConfigType *Com_FindIPdu(PduIdType id, uint16_t *indexOut)
{
    const Com_IPduConfigType *match = NULL;
    uint16_t i;
    for (i = 0U; i < COM_NUM_IPDUS; i++)
    {
        if (Com_IPduConfig[i].ipduId == id)
        {
            if (match != NULL) { return NULL; }
            match = &Com_IPduConfig[i];
            if (indexOut != NULL) { *indexOut = i; }
        }
    }
    return match;
}

/** Find exactly one group by local ID. */
static const Com_SignalGroupConfigType *Com_FindGroup(PduIdType id)
{
    const Com_SignalGroupConfigType *match = NULL;
    uint16_t i;
    for (i = 0U; i < COM_NUM_SIGNAL_GROUPS; i++)
    {
        if (Com_SignalGroupConfig[i].signalGroupId == id)
        {
            if (match != NULL) { return NULL; }
            match = &Com_SignalGroupConfig[i];
        }
    }
    return match;
}

/** Find exactly one Signal by local ID. */
static const Com_SignalConfigType *Com_FindSignal(PduIdType id)
{
    const Com_SignalConfigType *match = NULL;
    uint16_t i;
    for (i = 0U; i < COM_NUM_SIGNALS; i++)
    {
        if (Com_SignalConfig[i].signalId == id)
        {
            if (match != NULL) { return NULL; }
            match = &Com_SignalConfig[i];
        }
    }
    return match;
}

/** Resolve Signal -> Group -> one I-PDU without assuming IDs are indices. */
static const Com_IPduConfigType *Com_ResolveSignal(
    PduIdType id, const Com_SignalConfigType **signalOut, uint16_t *indexOut)
{
    const Com_SignalConfigType *signal = Com_FindSignal(id);
    const Com_IPduConfigType *ipdu = NULL;
    uint16_t i;
    if ((signal == NULL) || (Com_FindGroup(signal->signalGroupId) == NULL))
    {
        return NULL;
    }
    for (i = 0U; i < COM_NUM_IPDUS; i++)
    {
        if (Com_IPduConfig[i].signalGroupId == signal->signalGroupId)
        {
            if (ipdu != NULL) { return NULL; }
            ipdu = &Com_IPduConfig[i];
            if (indexOut != NULL) { *indexOut = i; }
        }
    }
    if (signalOut != NULL) { *signalOut = signal; }
    return ipdu;
}

/** Read one byte-aligned slot in little-endian byte order. */
static uint32_t Com_ReadSlot(const uint8_t *buffer, const Com_SignalConfigType *signal)
{
    uint32_t value = 0U;
    uint8_t b;
    uint8_t width = (uint8_t)(signal->slotLength / 8U);
    uint8_t start = (uint8_t)(signal->slotStartBit / 8U);
    for (b = 0U; b < width; b++)
    {
        value |= (uint32_t)buffer[start + b] << ((uint32_t)b * 8U);
    }
    return value;
}

/** Write only the configured slot in little-endian byte order. */
static void Com_WriteSlot(uint8_t *buffer, const Com_SignalConfigType *signal,
                          uint32_t value)
{
    uint8_t b;
    uint8_t width = (uint8_t)(signal->slotLength / 8U);
    uint8_t start = (uint8_t)(signal->slotStartBit / 8U);
    for (b = 0U; b < width; b++)
    {
        buffer[start + b] = (uint8_t)(value >> ((uint32_t)b * 8U));
    }
}

/** Validate hierarchy, IDs, byte slots, overlap and Tx timing before readiness. */
static Std_ReturnType Com_ValidateConfig(void)
{
    uint16_t i, j, k;
    for (i = 0U; i < COM_NUM_IPDUS; i++)
    {
        const Com_IPduConfigType *p = &Com_IPduConfig[i];
        if ((p->length == 0U) || (p->length > COM_MAX_IPDU_LENGTH) ||
            (Com_FindIPdu(p->ipduId, NULL) == NULL) ||
            (Com_FindGroup(p->signalGroupId) == NULL) ||
            ((p->direction != COM_IPDU_TX) && (p->direction != COM_IPDU_RX)) ||
            ((p->direction == COM_IPDU_TX) &&
             ((p->periodTicks == 0U) || (p->initialOffsetTicks == 0U))))
        { return E_NOT_OK; }
        for (j = 0U; j < i; j++)
        {
            if ((Com_IPduConfig[j].signalGroupId == p->signalGroupId) ||
                ((Com_IPduConfig[j].direction == p->direction) &&
                 (Com_IPduConfig[j].globalPduId == p->globalPduId)))
            { return E_NOT_OK; }
        }
    }
    for (i = 0U; i < COM_NUM_SIGNAL_GROUPS; i++)
    {
        const Com_SignalGroupConfigType *g = &Com_SignalGroupConfig[i];
        uint16_t owners = 0U;
        if ((Com_FindGroup(g->signalGroupId) == NULL) ||
            (g->numSignals == 0U) || (g->signalList == NULL))
        { return E_NOT_OK; }
        for (j = 0U; j < COM_NUM_IPDUS; j++)
        {
            if (Com_IPduConfig[j].signalGroupId == g->signalGroupId) { owners++; }
        }
        if (owners != 1U) { return E_NOT_OK; }
        for (j = 0U; j < g->numSignals; j++)
        {
            const Com_SignalConfigType *s = Com_FindSignal(g->signalList[j]);
            if ((s == NULL) || (s->signalGroupId != g->signalGroupId))
            { return E_NOT_OK; }
            for (k = 0U; k < j; k++)
            {
                if (g->signalList[k] == g->signalList[j]) { return E_NOT_OK; }
            }
        }
    }
    for (i = 0U; i < COM_NUM_SIGNALS; i++)
    {
        const Com_SignalConfigType *s = &Com_SignalConfig[i];
        const Com_IPduConfigType *p = Com_ResolveSignal(s->signalId, NULL, NULL);
        const Com_SignalGroupConfigType *g = Com_FindGroup(s->signalGroupId);
        uint16_t memberships = 0U;
        if ((Com_FindSignal(s->signalId) == NULL) || (p == NULL) || (g == NULL) ||
            (s->slotLength < 8U) || (s->slotLength > 32U) ||
            ((s->slotStartBit % 8U) != 0U) || ((s->slotLength % 8U) != 0U) ||
            ((uint32_t)s->slotStartBit + s->slotLength > (uint32_t)p->length * 8U))
        { return E_NOT_OK; }
        for (j = 0U; j < g->numSignals; j++)
        {
            if (g->signalList[j] == s->signalId) { memberships++; }
        }
        if (memberships != 1U) { return E_NOT_OK; }
        for (j = 0U; j < i; j++)
        {
            const Com_SignalConfigType *other = &Com_SignalConfig[j];
            if ((other->signalGroupId == s->signalGroupId) &&
                ((uint32_t)s->slotStartBit < (uint32_t)other->slotStartBit + other->slotLength) &&
                ((uint32_t)other->slotStartBit < (uint32_t)s->slotStartBit + s->slotLength))
            { return E_NOT_OK; }
        }
    }
    return E_OK;
}

/** Clear only the Update Bit at the start of each slot in one Tx group. */
static void Com_ClearUpdateBits(uint16_t ipduIndex)
{
    const Com_SignalGroupConfigType *g =
        Com_FindGroup(Com_IPduConfig[ipduIndex].signalGroupId);
    uint8_t i;
    for (i = 0U; i < g->numSignals; i++)
    {
        const Com_SignalConfigType *s = Com_FindSignal(g->signalList[i]);
        Com_Buffer[ipduIndex][s->slotStartBit / 8U] &= (uint8_t)~1U;
    }
}

/** Reject malformed configuration, then reset all COM buffers and timers. */
Std_ReturnType Com_Init(void)
{
    uint16_t i;
    Com_Initialized = 0U;
    Com_LogSequence = 0U;
    if (Com_ValidateConfig() != E_OK)
    {
        Com_Log(COM_LOG_CONFIG_ERROR, 0U, 0U);
        return E_NOT_OK;
    }
    memset(Com_Buffer, 0, sizeof(Com_Buffer));
    memset(Com_RxValid, 0, sizeof(Com_RxValid));
    memset(Com_TxRuntime, 0, sizeof(Com_TxRuntime));
    Com_TxConfirmationCount = 0U;
    Com_RxIndicationCount = 0U;
    Com_TxDropCount = 0U;
    for (i = 0U; i < COM_NUM_IPDUS; i++)
    {
        if (Com_IPduConfig[i].direction == COM_IPDU_TX)
        { Com_TxRuntime[i].counter = Com_IPduConfig[i].initialOffsetTicks; }
    }
    Com_Initialized = 1U;
    Com_Log(COM_LOG_INIT_OK, 0U, COM_NUM_IPDUS);
    return E_OK;
}

/** Copy a uint32_t input, encode its Tx Signal slot and set U without sending. */
Std_ReturnType Com_SendSignal(PduIdType SignalId, const void *SignalDataPtr)
{
    const Com_SignalConfigType *s;
    const Com_IPduConfigType *p;
    uint16_t index;
    uint8_t bits;
    uint32_t value;
    if ((Com_Initialized == 0U) || (SignalDataPtr == NULL)) { return E_NOT_OK; }
    p = Com_ResolveSignal(SignalId, &s, &index);
    if ((p == NULL) || (p->direction != COM_IPDU_TX)) { return E_NOT_OK; }
    memcpy(&value, SignalDataPtr, sizeof(value));
    bits = (uint8_t)(s->slotLength - 1U);
    if (value > ((1UL << bits) - 1UL)) { return E_NOT_OK; }
    Com_WriteSlot(Com_Buffer[index], s, (value << 1U) | 1U);
    return E_OK;
}

/** Write a decoded Rx Signal value to a caller-owned uint32_t. */
Std_ReturnType Com_ReceiveSignal(PduIdType SignalId, void *SignalDataPtr)
{
    const Com_SignalConfigType *s;
    const Com_IPduConfigType *p;
    uint16_t index;
    uint32_t slot;
    uint32_t value;
    if ((Com_Initialized == 0U) || (SignalDataPtr == NULL))
    { return E_NOT_OK; }
    p = Com_ResolveSignal(SignalId, &s, &index);
    if ((p == NULL) || (p->direction != COM_IPDU_RX) || (Com_RxValid[index] == 0U))
    { return E_NOT_OK; }
    slot = Com_ReadSlot(Com_Buffer[index], s);
    value = slot >> 1U;
    memcpy(SignalDataPtr, &value, sizeof(value));
    return E_OK;
}

/** Decrement nominal timers, retry pending frames once and preserve U on drop. */
void Com_MainFunctionTx(void)
{
    uint16_t i;
    if (Com_Initialized == 0U) { return; }
    for (i = 0U; i < COM_NUM_IPDUS; i++)
    {
        Com_TxRuntimeType *rt = &Com_TxRuntime[i];
        const Com_IPduConfigType *p = &Com_IPduConfig[i];
        PduInfoType info;
        if (p->direction != COM_IPDU_TX) { continue; }
        if (rt->counter > 0U) { rt->counter--; }
        if (rt->counter == 0U)
        {
            if (rt->pending == 0U) { rt->pending = 1U; rt->retryCount = 0U; }
            rt->counter = p->periodTicks;
        }
        if (rt->pending == 0U) { continue; }
        info.SduDataPtr = Com_Buffer[i];
        info.SduLength = p->length;
        if (PduR_ComTransmit(p->ipduId, &info) == E_OK)
        {
            rt->pending = 0U;
            rt->retryCount = 0U;
            Com_ClearUpdateBits(i);
            Com_Log(COM_LOG_TX_ACCEPTED, p->ipduId, p->length);
        }
        else if (rt->retryCount < p->maxRetries)
        {
            rt->retryCount++;
            Com_Log(COM_LOG_TX_RETRY, p->ipduId, rt->retryCount);
        }
        else
        {
            rt->pending = 0U;
            rt->retryCount = 0U;
            Com_TxDropCount++;
            Com_Log(COM_LOG_TX_DROP, p->ipduId, Com_TxDropCount);
        }
    }
}

/** Copy only a correctly sized Rx I-PDU; never retain the caller pointer. */
void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType *PduInfoPtr)
{
    uint16_t index;
    const Com_IPduConfigType *p = Com_FindIPdu(ComRxPduId, &index);
    if ((Com_Initialized == 0U) || (p == NULL) || (p->direction != COM_IPDU_RX) ||
        (PduInfoPtr == NULL) || (PduInfoPtr->SduLength != p->length) ||
        (PduInfoPtr->SduDataPtr == NULL))
    {
        if (Com_Initialized != 0U) { Com_Log(COM_LOG_RX_REJECTED, ComRxPduId, 0U); }
        return;
    }
    memcpy(Com_Buffer[index], PduInfoPtr->SduDataPtr, p->length);
    Com_RxValid[index] = 1U;
    Com_RxIndicationCount++;
    Com_Log(COM_LOG_RX_ACCEPTED, ComRxPduId, p->length);
}

/** Count accepted Tx completions without changing scheduling or Update Bits. */
void Com_TxConfirmation(PduIdType ComTxPduId)
{
    const Com_IPduConfigType *p = Com_FindIPdu(ComTxPduId, NULL);
    if ((Com_Initialized != 0U) && (p != NULL) && (p->direction == COM_IPDU_TX))
    { Com_TxConfirmationCount++; }
}

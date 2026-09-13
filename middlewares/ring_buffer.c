#include "ring_buffer.h"

#include <stddef.h>

/* Initialize ring buffer instance with memory array and capacity */
RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t capacity)
{
    if ((rb == NULL) || (capacity < 2) || (buffer == NULL))
    {
        return RING_BUFFER_NULL_PTR;
    }

    rb->buffer = buffer;
    rb->head = 0U;
    rb->tail = 0U;
    rb->capacity = capacity;

    return RING_BUFFER_OK;
}

/* Check if ring buffer has no elements */
bool RingBuffer_IsEmpty(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return true;
    }

    return (rb->head == rb->tail);
}

/* Check if ring buffer is at maximum capacity */
bool RingBuffer_IsFull(const RingBuffer_t *rb)
{
    if (rb == NULL)
    {
        return true;
    }

    return (((rb->head + 1U) % rb->capacity) == rb->tail);
}

/* Push a single byte into ring buffer */
RingBuffer_Status_t RingBuffer_Push(RingBuffer_t *rb, uint8_t data)
{
    /* Validate input arguments */
    if ((rb == NULL) || (rb->buffer == NULL))
    {
        return RING_BUFFER_NULL_PTR;
    }

    /* Check if buffer is full using helper function */
    if (RingBuffer_IsFull(rb))
    {
        return RING_BUFFER_FULL;
    }

    /* Store data at current head index */
    rb->buffer[rb->head] = data;

    /* Advance head index circularly */
    rb->head = (rb->head + 1U) % rb->capacity;

    return RING_BUFFER_OK;
}

/* Pop a single byte from ring buffer */
RingBuffer_Status_t RingBuffer_Pop(RingBuffer_t *rb, uint8_t *data)
{
    /* Validate input arguments */
    if ((rb == NULL) || (rb->buffer == NULL) || (data == NULL))
    {
        return RING_BUFFER_NULL_PTR;
    }

    /* Check if buffer has no data */
    if (RingBuffer_IsEmpty(rb))
    {
        return RING_BUFFER_EMPTY;
    }

    /* Retrieve byte from current tail position */
    *data = rb->buffer[rb->tail];

    /* Advance tail index circularly */
    rb->tail = (rb->tail + 1U) % rb->capacity;

    return RING_BUFFER_OK;
}

/* Reset head and tail indices to clear buffer */
void RingBuffer_Clear(RingBuffer_t *rb)
{
    /* Validate input argument */
    if (rb != NULL)
    {
        rb->head = 0U;
        rb->tail = 0U;
    }
}

/* Get current count of available bytes in buffer */
uint16_t RingBuffer_GetCount(const RingBuffer_t *rb)
{
    /* Validate input argument */
    if (rb == NULL)
    {
        return 0U;
    }

    /* Calculate count considering circular wraparound */
    return (uint16_t)((rb->head + rb->capacity - rb->tail) % rb->capacity);
}

/* Get number of bytes that can still be stored */
uint16_t RingBuffer_GetFree(const RingBuffer_t *rb)
{
    if ((rb == NULL) || (rb->capacity < 2U))
    {
        return 0U;
    }

    return (uint16_t)((rb->capacity - 1U) - RingBuffer_GetCount(rb));
}

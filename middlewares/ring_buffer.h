#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

/* Return status codes for ring buffer operations */
typedef enum {
    RING_BUFFER_OK          = 0,    /* Operation completed successfully */
    RING_BUFFER_EMPTY       = 1,    /* Buffer is empty, no data to read */
    RING_BUFFER_FULL        = 2,    /* Buffer is full, cannot write data */
    RING_BUFFER_NULL_PTR    = 3     /* Provided pointer argument is NULL */
} RingBuffer_Status_t;

/* Ring buffer control structure */
typedef struct {
    uint8_t *buffer;                /* Pointer to raw memory array */
    volatile uint16_t head;         /* Write index modified on push */
    volatile uint16_t tail;         /* Read index modified on pop */
    uint16_t capacity;              /* Maximum storage size of buffer */
} RingBuffer_t;

/* Initialize ring buffer instance with memory array and capacity */
RingBuffer_Status_t RingBuffer_Init(RingBuffer_t *rb, uint8_t *buffer, uint16_t capacity);

/* Push a single byte into ring buffer */
RingBuffer_Status_t RingBuffer_Push(RingBuffer_t *rb, uint8_t data);

/* Pop a single byte from ring buffer */
RingBuffer_Status_t RingBuffer_Pop(RingBuffer_t *rb, uint8_t *data);

/* Check if ring buffer has no elements */
bool RingBuffer_IsEmpty(const RingBuffer_t *rb);

/* Check if ring buffer is at maximum capacity */
bool RingBuffer_IsFull(const RingBuffer_t *rb);

/* Reset head and tail indices to clear buffer */
void RingBuffer_Clear(RingBuffer_t *rb);

/* Get current count of available bytes in buffer */
uint16_t RingBuffer_GetCount(const RingBuffer_t *rb);

/* Get number of bytes that can still be stored */
uint16_t RingBuffer_GetFree(const RingBuffer_t *rb);

#endif /* RING_BUFFER_H_ */

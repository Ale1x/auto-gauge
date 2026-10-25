#ifndef AG_TEST_QUEUE_H
#define AG_TEST_QUEUE_H
#include "FreeRTOS.h"
typedef struct {size_t capacity,size,head,count;uint8_t *storage;} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(size_t,size_t,uint8_t *,StaticQueue_t *);
BaseType_t xQueueSendFromISR(QueueHandle_t,const void *,BaseType_t *);
BaseType_t xQueueReceive(QueueHandle_t,void *,TickType_t);
#endif

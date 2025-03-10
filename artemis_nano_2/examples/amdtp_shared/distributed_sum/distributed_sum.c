#include "distributed_sum.h"
#include "am_util_debug.h"
#include "am_util_stdio.h"
#include "FreeRTOS.h"
#include "task.h"

int randomData[APP_TASK_COUNT];
int resultData[APP_TASK_COUNT];


void initClientTasks(Task *tasks, size_t *numTasks) {
    
    *numTasks = APP_TASK_COUNT;

    for (int i = 0; i < APP_TASK_COUNT; i++) {
        tasks[i].data = &randomData[i];
        randomData[i] = i;
        tasks[i].dataLength = sizeof(int);
        tasks[i].result = &resultData[i];
    }
}

void initServerTask(Task *task) {
    task->data = &randomData[0];
    task->dataLength = sizeof(int);
    task->result = &resultData[0];
}

void copyTaskDataToSendBuffer(uint8_t *buffer, Task *task) {
    am_util_stdio_printf("Copying task data to send buffer\n");
    memcpy(buffer, task->data, task->dataLength);
}

void reassembleTaskResults(Task *tasks, size_t numTasks) {
    if (numTasks < APP_TASK_COUNT) {
        am_util_debug_printf("Not all tasks are complete...\n");
    }

    int sum = 0;

    am_util_stdio_printf("Summing all tasks...\n");
    for (int i = 0; i < numTasks; i++) {
        sum += *((int *) tasks[i].result);
    }

    am_util_stdio_printf("Sum of all tasks: %d\n", sum);
}

void executeTask(Task *task) {
    am_util_stdio_printf("Executing task %d\n", task->taskId);
    int *data = (int *) task->data;
    // am_util_debug_printf("successfully declared *data %x\n", data);
    am_util_debug_printf("value of *data %d\n", *data);

    int *result = (int *) task->result;
    // am_util_debug_printf("successfully declared *result  %x\n", result);


    *result = 0;
    // am_util_debug_printf("successfully assigned *result again  %x\n", result);

    *result = *data * 10;
    // am_util_debug_printf("entering delay\n");
    vTaskDelay(1000);

    // am_util_debug_printf("exiting delay\n");
    task->status = DP_TASK_STATUS_COMPLETE;
    distributionProtocolTaskHandle = NULL;
    am_util_stdio_printf("Task complete, result = %d\n", *result);
    vTaskDelete(NULL);
}
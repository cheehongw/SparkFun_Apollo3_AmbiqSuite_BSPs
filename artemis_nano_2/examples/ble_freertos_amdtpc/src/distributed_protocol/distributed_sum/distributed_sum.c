#include "distributed_sum.h"
#include "am_util_debug.h"

int randomData[APP_TASK_COUNT];
int resultData[APP_TASK_COUNT];


void initializeSumTask(Task *tasks, size_t *numTasks) {
    
    *numTasks = APP_TASK_COUNT;

    for (int i = 0; i < APP_TASK_COUNT; i++) {
        tasks[i].data = &randomData[i];
        tasks[i].dataLength = sizeof(int);
        tasks[i].result = &resultData[i];
    }
}


void reassembleSumTask(Task *tasks, size_t numTasks) {
    if (numTasks < APP_TASK_COUNT) {
        am_util_debug_printf("Not all tasks are complete...\n");
    }

    int sum = 0;
    int res = 0;

    am_util_debug_printf("Summing all tasks...\n");
    for (int i = 0; i < numTasks; i++) {
        sum += *((int *) tasks[i].result);
    }

    am_util_debug_printf("Sum of all tasks: %d\n", sum);
}
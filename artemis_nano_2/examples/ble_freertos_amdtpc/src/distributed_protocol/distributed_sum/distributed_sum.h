#include "distributed_protocol.h"

#define APP_TASK_COUNT 3

void initializeSumTask(Task *tasks, size_t *numTasks);
void reassembleSumTask(Task *tasks, size_t numTasks);
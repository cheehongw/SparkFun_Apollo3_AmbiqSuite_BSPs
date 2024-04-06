#ifndef DISTRIBUTED_PROTOCOL_H
#define DISTRIBUTED_PROTOCOL_H

#include "dm_api.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "amdtpc_api.h"

typedef enum eDpPktType {
    DP_PKT_TYPE_UNKNOWN,
    DP_PKT_TYPE_NEW_TASK,       // TYPE + TASK_ID + LEN + DATA
    DP_PKT_TYPE_RESPONSE,       // TYPE + TASK_ID + LEN + STATUS + DATA
    DP_PKT_TYPE_ENQUIRY,
    DP_PKT_TYPE_MAX
} eDpPktType_t;


typedef enum eDpTaskStatus {
    DP_TASK_STATUS_UNKNOWN,
    DP_TASK_STATUS_INCOMPLETE,
    DP_TASK_STATUS_IN_PROGRESS,
    DP_TASK_STATUS_COMPLETE,
    DP_TASK_STATUS_MAX
} eDpTaskStatus_t;


#define DP_LEN_SIZE                 sizeof(uint8_t)
#define DP_STATUS_SIZE              sizeof(eDpTaskStatus_t)
#define DP_TASK_ID_SIZE             sizeof(uint8_t)
#define DP_PKT_TYPE_SIZE            sizeof(eDpPktType_t)

#define DP_ENQUIRY_PKT_SIZE         DP_PKT_TYPE_SIZE + DP_TASK_ID_SIZE
#define DP_NEW_TASK_HEADER_SIZE     DP_PKT_TYPE_SIZE + DP_TASK_ID_SIZE + DP_LEN_SIZE + DP_STATUS_SIZE
#define DP_RESPONSE_HEADER_SIZE     DP_PKT_TYPE_SIZE + DP_TASK_ID_SIZE + DP_LEN_SIZE + DP_STATUS_SIZE


typedef struct {
    eDpPktType_t type;
    uint8_t taskId;
    uint8_t len;                    // Length of the data   
    eDpTaskStatus_t status;
    void *data;
} distributedProtocolPacket_t;

typedef struct {
    uint8_t taskId;
    eDpTaskStatus_t status;
    void *data;
    int dataLength;
    void *result;                             // Store the result of the task
    // Add any other task-related data here
} Task;


typedef struct {
    dmConnId_t          connId;                 // Connection ID of the client
    Task*               assignedTask;           // Task assigned to the client
    SemaphoreHandle_t   receivedReplySem;       // Flag to indicate if the client has replied
    StaticSemaphore_t   xSemaphoreBuffer; // Semaphore structure
} Client;

// application layer to initialize tasks
// sets the memory location for data and results
// typedef void (*dp_initialize_tasks_t)(Task* tasks, size_t* numTasks);
// typedef void (*dp_reassemble_task_results_t)(Task* tasks, size_t numTasksCompleted);

void doDistributedTask();
void initializeDistributedProtocol();
void addConnectedClient(dmConnId_t connId);
void removeConnectedClient(dmConnId_t connId);
void DpRecvCb(uint8_t *buf, uint16_t len, dmConnId_t connId);

extern void executeTask(Task *task);
extern void initClientTasks(Task *tasks, size_t *numTasks);
extern void initServerTask(Task *task);
extern void reassembleTaskResults(Task *tasks, size_t numTasksCompleted);


extern TaskHandle_t distributionProtocolTaskHandle;

#endif // DISTRIBUTED_PROTOCOL_H
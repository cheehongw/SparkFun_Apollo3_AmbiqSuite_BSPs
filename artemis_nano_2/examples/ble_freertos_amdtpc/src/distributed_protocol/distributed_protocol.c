#include "distributed_protocol.h"
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"


#include "am_util_stdio.h"
#include "am_util_debug.h"

#define MAX_TASKS 50

TaskHandle_t distributionProtocolTaskHandle;

Task tasks[MAX_TASKS + 1];
Client connectedClients[DM_CONN_MAX];

size_t taskCount;
dp_reassemble_task_results_t customReassembleTaskResults;         // Application defined function to reassemble task results
dp_initialize_tasks_t customInitializeTasks;                // Application defined function to initialize tasks  

uint8_t dpBuf[1024];                                        // Buffer to store the distributed protocol packet
Task* taskQueue[MAX_TASKS + 1];
int head = 0;
int tail = 0;

// --------------------------------------------------------------------------------------------

bool isTaskQueueEmpty() {
    return head == tail;
}

bool enqueueTask(Task* task) {
    if ((tail + 1) % MAX_TASKS == head) {
        // Queue is full
        am_util_debug_printf("Task queue is full, cannot add task\n");
        return false;
    }

    taskQueue[tail] = task;
    tail = (tail + 1) % MAX_TASKS;
    return true;
}

void addTaskBackToQueue(Task *task) {
    if (!enqueueTask(task)) {
        while(1); // Queue is full, this should not happen!
    } // Add the task back to the task queue
}


Task* dequeueTask() {
    if (isTaskQueueEmpty()) {
        // Queue is empty
        return NULL;
    }

    Task* task = taskQueue[head];
    head = (head + 1) % MAX_TASKS;
    return task;
}

// --------------------------------------------------------------------------------------------


void initializeTasks() {
    // Call the application defined function to initialize the location to store data and result
    am_util_debug_printf("Initializing distributed tasks...\n");
    customInitializeTasks(tasks, &taskCount); 

    if (taskCount > MAX_TASKS) {
        am_util_debug_printf("Too many tasks for dp to handle, this should not happen\n");
        while(1); // Too many tasks, this should not happen
    }

    for (int i = 0; i < taskCount; i++) {
        tasks[i].taskId = i;
        tasks[i].status = DP_TASK_STATUS_INCOMPLETE;
        // am_util_debug_printf("Task %d initialized\n", i);
        if (!enqueueTask(&tasks[i])) {
            while(1); // Queue is full, this should not happen
        }; // Add the incomplete task to the task queue
    }
}

/**
 * @brief Builds a distributed protocol packet
 * 
 * @param type The packet type
 * @param task The task
 * @param buf The buffer that the data is located in
 * @param status The task status (if any)
 * 
 * @return The length of the packet
 */
uint16_t DpBuildPacket(uint8_t type, Task *task, uint8_t *buf, eDpTaskStatus_t status) {
    am_util_debug_printf("Building packet of type %d\n", type);

    distributedProtocolPacket_t *pkt;

    pkt = (distributedProtocolPacket_t *) buf;

    pkt->type = type;
    pkt->taskId = task->taskId;
    // Build the packet
    if (type == DP_PKT_TYPE_ENQUIRY) {
        return DP_ENQUIRY_PKT_SIZE;
    } else if (type == DP_PKT_TYPE_NEW_TASK) {
        pkt->len = task->dataLength;
        am_util_debug_printf("Task data length: %d\n", task->dataLength);


        am_util_debug_printf("Pointer to task data: %x\n", task->data);
        am_util_debug_printf("Pointer to pkt task data: %x\n", &(pkt->taskData.data));
        memcpy(&(pkt->taskData.data), task->data, task->dataLength);
        am_util_debug_printf("memcpy data");
        return DP_NEW_TASK_HEADER_SIZE + task->dataLength;


    } else if (type == DP_PKT_TYPE_RESPONSE) {
        pkt->len = task->dataLength;

        if (status == DP_TASK_STATUS_COMPLETE) {
            pkt->taskData.statusWithData.status = status;
            memcpy(&(pkt->taskData.statusWithData.data), task->result, task->dataLength);
            return DP_RESPONSE_HEADER_SIZE + task->dataLength;
        } 

        pkt->taskData.status = status;
        return DP_RESPONSE_HEADER_SIZE;
    }

    return 0;
}

/**
 * @brief Callback function for receiving distributed protocol packets
 * 
 * @param buf The buffer containing the packet, usually from the transport layer protocol
 * @param len The len of the packet
 * @param connId The connection ID of the slave device
 */
void DpRecvCb(uint8_t *buf, uint16_t len, dmConnId_t connId) {

    if (len < DP_ENQUIRY_PKT_SIZE) {
        // Packet is too short, thats wrong
        am_util_debug_printf("Packet is too short, thats wrong. len: %d", len);
        while (1);
    }

    distributedProtocolPacket_t *pkt = (distributedProtocolPacket_t *) buf;
    eDpPktType_t type = pkt->type;

    if (type == DP_PKT_TYPE_RESPONSE) { //should only have this for master device
        Task *task = &tasks[pkt->taskId];
        eDpTaskStatus_t status = pkt->taskData.statusWithData.status;
        am_util_debug_printf("Received response from client %d for task %d, task status: %d\n", connId, task->taskId, status);

        if (status == DP_TASK_STATUS_COMPLETE) {
            uint8_t resultLen = pkt->len;
            // am_util_debug_printf("Pointer to task result: %x\n", task->result);
            // am_util_debug_printf("Pointer to pkt task result: %x\n", &(pkt->taskData.statusWithData.data));
            memcpy(task->result, &(pkt->taskData.statusWithData.data), resultLen);

            task->status = DP_TASK_STATUS_COMPLETE;
            connectedClients[connId - 1].assignedTask = NULL; // Remove the task from the client

        } else if (status == DP_TASK_STATUS_IN_PROGRESS) {
            //still in progress
            // upgrade with time out checks later
            // do nothing and continue for now
        } else if (status == DP_TASK_STATUS_UNKNOWN) {
            //task failed, or slave is not working on this task
            task->status = DP_TASK_STATUS_INCOMPLETE;
            addTaskBackToQueue(task); // Add the task back to the task queue
        }
        xSemaphoreGive(connectedClients[connId - 1].receivedReplySem); // Set the receivedReplySem flag for the client
    }
}

void sendTaskToClient(Client *client, Task *task) {

    am_util_debug_printf("Sending task %d to client %d\n", task->taskId, client->connId);
    task->status = DP_TASK_STATUS_IN_PROGRESS;
    client->assignedTask = task;
    uint16_t overallPacketLength = DpBuildPacket(DP_PKT_TYPE_NEW_TASK, task, dpBuf, task->dataLength);

    am_util_debug_printf("Invoking amdtpc send for task %d to client %d\n", task->taskId, client->connId);
    AmdtpcSendPacket(AMDTP_PKT_TYPE_DATA, 0, 1, dpBuf, overallPacketLength, client->connId);
    
}

/**
 * @brief Assigns a task to each client that is free
 */
int sendTasksToClients() {
    int tasksSent = 0;
    for (int i = 0; i < DM_CONN_MAX; i++) {
        if (connectedClients[i].connId == 0 || connectedClients[i].assignedTask != NULL) {
            continue;
        }

        Task *task = dequeueTask();                     // Get the task from the task queue
        if (task == NULL) {
            am_util_debug_printf("No tasks in the queue, exiting sendTasksToClients...\n");
            break;
        }

        sendTaskToClient(&connectedClients[i], task);   // Send the task to the client
        tasksSent++;
    }

    return tasksSent;

}

void pollClient(Client *client) {
    uint16_t overallPacketLength;
    overallPacketLength = DpBuildPacket(DP_PKT_TYPE_ENQUIRY, client->assignedTask, dpBuf, 0);
    am_util_debug_printf("Polling client %d\n", client->connId);
    while (AmdtpcSendPacket(AMDTP_PKT_TYPE_DATA, 0, 1, dpBuf, overallPacketLength, client->connId) != AMDTP_STATUS_SUCCESS);
    am_util_debug_printf("Poll request sent to client %d for task %d, waiting for reply before polling others...\n", client->connId, client->assignedTask->taskId);
    xSemaphoreTake(client->receivedReplySem, portMAX_DELAY);
    // will block until the semaphore is given in the recv callback
}

void pollClientsForReplies() {
    for (int i = 0; i < DM_CONN_MAX; i++) {
        if (connectedClients[i].connId == 0 || connectedClients[i].assignedTask == NULL) {
            continue;
        }

        pollClient(&connectedClients[i]); // Call pollClient() for each client
    }
}

bool areAllTasksCompleted() {
    for (int i = 0; i < taskCount; i++) {
        if (tasks[i].status != DP_TASK_STATUS_COMPLETE) {
            return false;
        }
    }
    return true;
}

int areClientsConnected() {
    int numConnectedClients = 0;

    for (int i = 0; i < DM_CONN_MAX; i++) {
        if (connectedClients[i].connId != 0) {
            numConnectedClients++;
            am_util_debug_printf("address: %x, connId: %d\n", connectedClients[i], connectedClients[i].connId);

        }
    }

    am_util_debug_printf("Number of connected clients: %d\n", numConnectedClients);

    return numConnectedClients;
}


void doDistributedTask() {
    initializeTasks();

    if (areClientsConnected() == 0) {
        am_util_debug_printf("No clients connected, exiting distributed task...\n");
        vTaskDelete(NULL); //task complete, stop the task...
        return;
    }
    
    sendTasksToClients();

    while (!areAllTasksCompleted()) {
        pollClientsForReplies();
        if (sendTasksToClients() == 0) {
            am_util_debug_printf("No tasks sent, inserting extra time...\n");
            
            vTaskDelay(1000); // Wait for 0.3 seconds before polling clients again
        };
    }

    // Reassemble results
    am_util_debug_printf("Reassembling task results... %x\n", customReassembleTaskResults);
    customReassembleTaskResults(tasks, taskCount);

    vTaskDelete(NULL); //task complete, stop the task...
}

void addConnectedClient(dmConnId_t connId) {
    // Add the client to the list
    connectedClients[connId - 1].connId = connId;
    connectedClients[connId - 1].assignedTask = NULL;   
    connectedClients[connId - 1].receivedReplySem = xSemaphoreCreateBinaryStatic(&(connectedClients[connId - 1].xSemaphoreBuffer));

    if (connectedClients[connId - 1].receivedReplySem == NULL) {
        // Semaphore creation failed
        am_util_debug_printf("Semaphore creation failed");
    }
}

void removeConnectedClient(dmConnId_t connId) {
    // Remove the client from the list
    connectedClients[connId - 1].connId = 0;
    connectedClients[connId - 1].assignedTask = NULL;
    vSemaphoreDelete(connectedClients[connId - 1].receivedReplySem);
}

void initializeDistributedProtocol(dp_initialize_tasks_t initializeTasks, dp_reassemble_task_results_t reassembleTaskResults) {
    // Initialize the distributed protocol
    am_util_debug_printf("Initializing distributed protocol...\n");
    for (int i = 0; i < DM_CONN_MAX; i++) {
        connectedClients[i].connId = 0;
        connectedClients[i].assignedTask = NULL;
        am_util_debug_printf("address: %x, connId: %d\n", connectedClients[i], connectedClients[i].connId);

    }

    taskCount = 0;
    customInitializeTasks = initializeTasks;
    customReassembleTaskResults = reassembleTaskResults;
}
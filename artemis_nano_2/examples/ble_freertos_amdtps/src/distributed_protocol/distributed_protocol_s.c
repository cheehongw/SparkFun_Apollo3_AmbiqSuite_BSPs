#include "distributed_protocol.h"
#include <stdbool.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "amdtp_common.h"
#include "amdtps_api.h"
#include "am_util_stdio.h"
#include "am_util_debug.h"

#define MAX_TASKS 10

TaskHandle_t distributionProtocolTaskHandle;

Task task;


uint8_t dpBuf[1024];


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
uint16_t DpBuildPacket(uint8_t type, Task *task, uint8_t *buf) {
    // am_util_debug_printf("Building packet of type %d\n", type);

    distributedProtocolPacket_t *pkt;

    pkt = (distributedProtocolPacket_t *) buf;

    pkt->type = type;
    pkt->taskId = task->taskId;
    // Build the packet
    if (type == DP_PKT_TYPE_RESPONSE) {
        pkt->len = task->dataLength;
        am_util_debug_printf("Building response packet ");

        if (task->status == DP_TASK_STATUS_COMPLETE) {
            pkt->taskData.statusWithData.status = task->status;
            am_util_debug_printf("for completed task %d, \n", task->taskId);
            // am_util_debug_printf("Pointer to task result: %x\n", task->result);
            // am_util_debug_printf("Pointer to pkt task data: %x\n", &(pkt->taskData.statusWithData.data));
            memcpy(&(pkt->taskData.statusWithData.data), task->result, task->dataLength);
            return DP_RESPONSE_HEADER_SIZE + task->dataLength;
        } 
        am_util_debug_printf("for unfinished task %d, \n", task->taskId);
        pkt->len = 0;
        pkt->taskData.statusWithData.status = task->status;
        return DP_RESPONSE_HEADER_SIZE;

    } else {
        am_util_debug_printf("Building unknown packet for slave????\n");
        while(1);
        return 0;
    }
}


void runExecuteTask(void *pvParameters) {
    Task *task = (Task *) pvParameters;
    am_util_debug_printf("Running task %d\n", task->taskId);
    executeTask(task);
}

/**
 * @brief Callback function for receiving distributed protocol packets
 * 
 * @param buf The buffer containing the packet, usually from the transport layer protocol
 * @param len The len of the amdtp packet
 * @param connId The connection ID of the slave device
 */
void DpRecvCb(uint8_t *buf, uint16_t len, dmConnId_t connId) {
    // am_util_debug_printf("Received data from transport layer\n");

    if (len < DP_ENQUIRY_PKT_SIZE) {
        // Packet is too short, thats wrong
        am_util_debug_printf("Packet is too short, thats wrong. len: %d", len);
        while (1);
    }

    distributedProtocolPacket_t *DpPkt = (distributedProtocolPacket_t *) buf;
    eDpPktType_t type = DpPkt->type;

    if (type == DP_PKT_TYPE_ENQUIRY) {
        am_util_debug_printf("Received enquiry for task %d\n", DpPkt->taskId);
        // build response packet using task
        uint16_t overallPacketLength = DpBuildPacket(DP_PKT_TYPE_RESPONSE, &task, dpBuf);
        AmdtpsSendPacket(AMDTP_PKT_TYPE_DATA, 0, 1, dpBuf, overallPacketLength, connId);

    } else if (type == DP_PKT_TYPE_NEW_TASK) {
        am_util_debug_printf("Received new task for task %d\n", DpPkt->taskId);

        if (task.status != DP_TASK_STATUS_IN_PROGRESS) {

            //receive the new task
            task.taskId = DpPkt->taskId;


            // am_util_debug_printf("Pointer to task data: %x\n", task.data);
            // am_util_debug_printf("Pointer to pkt task data: %x\n", &(DpPkt->taskData.data));
            memcpy(task.data, &(DpPkt->taskData.data), DpPkt->len);
            task.status = DP_TASK_STATUS_IN_PROGRESS;

            xTaskCreate(runExecuteTask, "Task", 1024, &task, 1, &distributionProtocolTaskHandle);
            return;
        } else {
            am_util_debug_printf("Received Task %d but task %d is already in progress\n", DpPkt->taskId, task.taskId);
        }
    }
}

void initializeDistributedProtocol() {
    am_util_debug_printf("Initializing distributed protocol\n");
    initializeTaskServer(&task);
}
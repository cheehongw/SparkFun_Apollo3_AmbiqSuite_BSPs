#include "matrix_mult.h"
#include "am_util_debug.h"
#include "am_util_stdio.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdlib.h>

#define DP_MASTER 1
#define DP_SLAVE 1

#ifdef DP_MASTER
// int MATRIX_A[M][P] = {
//     {1, 2},
//     {3, 4},
//     {5, 6}
// };     // the i,j element of A is A[i][j]
// int MATRIX_B[N][P] = {
//     {1, 4},
//     {2, 5},
//     {3, 6}
// };      // the i,j element of B is B[j][i]
int MATRIX_A[M][P];
int MATRIX_B[N][P];
int MATRIX_C[M][N];     // the i,j element of C is C[j][i]
#endif

#ifdef DP_SLAVE
int matrixData[2*P];
int result;
#endif

void identityMatrix(int* matrix, int row, int col) {
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            if (i == j) {
                matrix[i * col + j] = 1;
            } else {
                matrix[i * col + j] = 0;
            }
        }
    }
}

void populateMatrix(int* matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i * cols + j] = rand() % 10;
        }
    }
}


void printMatrixTranspose(int* matrix, int rows, int cols) {
    for(int i = 0; i < rows; i++) {
        for(int j = 0; j < cols; j++) {
            am_util_stdio_printf("%d ", matrix[j * rows + i]);
        }
        am_util_stdio_printf("\n");
    }
}

void printMatrix(int* matrix, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            am_util_stdio_printf("%d ", matrix[i * cols + j]);
        }
        am_util_stdio_printf("\n");
    }
}

/**
 * @brief   Initialize the memory for the tasks and the data to be used in the tasks.
 *          This function is called by the client to set the pointers task[i].data and task[i].result
 *          to point to the memory allocated for the data and result
 * 
 * @param tasks The task array in the distributed protocol
 * @param numTasks Number of tasks
 */
void initClientTasks(Task *tasks, size_t *numTasks) {
    
    *numTasks = M*N;
    
    populateMatrix((int *)MATRIX_A, M, P);
    identityMatrix((int *)MATRIX_B, N, P);

    printMatrix((int *)MATRIX_A, M, P);
    
    am_util_stdio_printf("\n");

    printMatrixTranspose((int *)MATRIX_B, N, P);

    for (int j = 0; j < N; j++) {  // for each col of B
        for (int i = 0; i < M; i++) {  //for each row of A
            int taskId = j + (i * N);  // taskId = col + (row * N)
            tasks[taskId].taskId = taskId;   
            tasks[taskId].data = NULL;
            tasks[taskId].dataLength = sizeof(int[P]) * 2;
            tasks[taskId].result = &(MATRIX_C[i][j]);
        }
    }
}


void copyTaskDataToSendBuffer(uint8_t *buffer, Task *task) {

    // am_util_stdio_printf("Copying task data to send buffer\n");
    int taskId = task->taskId;
    int i = taskId / N;
    int j = taskId % N;
    memcpy(buffer, MATRIX_A[i], sizeof(int[P]));
    memcpy(buffer + sizeof(int[P]), MATRIX_B[j], sizeof(int[P]));
    // am_util_stdio_printf("Copying task data to send buffer finished\n");
}


/**
 * @brief   Initialize the memory for the task and the data to be used in the task.
 *          This function is called by the server to set the pointers task.data and task.result
 *          to point to the memory allocated for the data and result
 * 
 *          This is needed becauses the distributed protocol is not concerned with what task is being executed.
 *          The application defines the size of the task and data and results used.
 * 
 * @param tasks The task array in the distributed protocol
 * @param numTasks Number of tasks
 */
void initServerTask(Task *task) {
    task->data = matrixData;
    task->dataLength = sizeof(int[P]) * 2;
    task->result = &result;
}


/**
 * @brief Reassembles the results
 * 
 * @param tasks 
 * @param numTasks 
 */
void reassembleTaskResults(Task *tasks, size_t numTasks) {
    if (numTasks < APP_TASK_COUNT) {
        am_util_debug_printf("Not all tasks are complete...\n");
    }

    am_util_stdio_printf("Printing matrix..\n");
    printMatrix((int *)MATRIX_C, M, N);

}

/**
 * @brief executes the results
 * 
 * @param task 
 */
void executeTask(Task *task) {
    am_util_stdio_printf("Executing task %d\n", task->taskId);
    int taskId = task->taskId;
    int i = taskId / N;
    int j = taskId % N;
    am_util_stdio_printf("Multiplying row %d of A with column %d of B\n", i, j);
    
    result = 0;

    for (int p = 0; p < P; p++) {
        result += matrixData[p] * matrixData[P + p];
    }

    task->status = DP_TASK_STATUS_COMPLETE;
    task->dataLength = sizeof(int);
    distributionProtocolTaskHandle = NULL;
    am_util_stdio_printf("Task complete, result = %d\n", result);

    vTaskDelete(NULL);
}
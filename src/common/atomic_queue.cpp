/*
 * atomic_queue.cpp
 * Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

#include "atomic_queue.h"
#include "fam_libfabric.h"
#include <allocator/memserver_allocator.h>
#include <atomic>
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <map>
#include <pthread.h>
#include <time.h>
using namespace std;
namespace openfam {

int numAtomicThreads;
uint64_t memoryPerThread;
int queueCapacity;
Fam_Ops_Libfabric *famOpsLibfabricQ;
atomicQueue atomicQ[MAX_ATOMIC_THREADS];
tInfo atomicTInfo[MAX_ATOMIC_THREADS];
pthread_mutex_t mutex[MAX_ATOMIC_THREADS] = {PTHREAD_MUTEX_INITIALIZER};
pthread_cond_t empty[MAX_ATOMIC_THREADS] = {PTHREAD_COND_INITIALIZER};
pthread_mutex_t pushQmutex[MAX_ATOMIC_THREADS] = {PTHREAD_MUTEX_INITIALIZER};
pthread_t atid[MAX_ATOMIC_THREADS];
void *atomicRegionIdRoot;

/* Create the queues, called by the memory server */
int atomicQueue::create(Memserver_Allocator *in_allocator,
                        const uint32_t in_qid) {
    void *localPointerQ;
    uint64_t offsetQ, offsetArray, offsetM, offsetA;
    bool qExists = true;
    assert(in_allocator != NULL);
    allocator = in_allocator;
    qId = in_qid;

    // find/create the queue data item
    if (*((uint64_t *)atomicRegionIdRoot + qId) == 0) {
        // Queue doesn't exist; allocate and update the offset
        try {
            offsetQ = allocator->allocate(ATOMIC_REGION_ID, sizeof(qData));
        } catch (...) {
            return ALLOCATEERROR;
        }
        *((uint64_t *)atomicRegionIdRoot + qId) = offsetQ;
        qExists = false;
    } else
        offsetQ = *((uint64_t *)atomicRegionIdRoot + qId);
    try {
        localPointerQ = allocator->get_local_pointer(ATOMIC_REGION_ID, offsetQ);
    } catch (...) {
        return MAPERROR;
    }
    if (!qExists)
        memset(localPointerQ, 0, sizeof(qData));
    offsetArray = sizeof(struct qData) - sizeof(uint64_t);
    // if Array and request records doesn't exists create
    if (*(uint64_t *)((char *)localPointerQ + offsetArray) == 0) {
        struct qData lcqData;
        // create the _Array data item to hold the offsets for queued messages
        try {
            offsetA = allocator->allocate(ATOMIC_REGION_ID,
                                          sizeof(uint64_t) * queueCapacity);
        } catch (...) {
            return ALLOCATEERROR;
        }
        // queue created. Update the queue information
        lcqData.front = lcqData.rear = 0; // lcqData.size = 0;
        lcqData.size.store(0, std::memory_order_acq_rel);
        lcqData.capacity = queueCapacity;
        lcqData.offsetArray = offsetA;
        memcpy(localPointerQ, &lcqData, sizeof(lcqData));
        try {
            pointerA = allocator->get_local_pointer(ATOMIC_REGION_ID, offsetA);
        } catch (...) {
            return MAPERROR;
        }
        uint32_t msgInd;
        // Allocate FAM data items for incoming requests and update in Array
        for (msgInd = 0; msgInd < lcqData.capacity; msgInd++) {
            try {
                offsetM =
                    allocator->allocate(ATOMIC_REGION_ID, sizeof(atomicMsg));
            } catch (...) {
                return ALLOCATEERROR;
            }
            *((uint64_t *)pointerA + msgInd) = offsetM;
        }
    }
    pointerQ = localPointerQ;
    struct qData *lcqData = (struct qData *)pointerQ;
    if (qExists)
        try {
            pointerA = allocator->get_local_pointer(ATOMIC_REGION_ID,
                                                    lcqData->offsetArray);
        } catch (...) {
            return MAPERROR;
        }

    std::cout << "queue creation successfull " << qId << std::endl;
    std::cout << "queue front " << lcqData->front << std::endl;
    std::cout << "queue rear " << lcqData->rear << std::endl;
    std::cout << "queue capacity " << lcqData->capacity << std::endl;
    std::cout << "queue size " << lcqData->size.load(std::memory_order_acq_rel)
              << std::endl;

    try {
        openfam_persist(pointerQ, sizeof(qData));
        openfam_persist(pointerA, sizeof(uint64_t) * queueCapacity);
    } catch (...) {
        return PERSISTERROR;
    }

    return 0;
}

/* Push element into the queue
 * get the rear pointer and populate the input message
 * In case of queue full throw exception
 * For scatter, gather and when rpc message contains data
 * allocate memory for the specified size
 * @param inpMsg - struct atomicMsg
 * @param inpDataSG - void *
 * @return - {success(0), failure(1), errNo(<0)}
 */
int atomicQueue::push(atomicMsg *inpMsg, void *inpDataSG) {
    uint64_t offsetDSG, qSize;
    ostringstream message;
    void *localPointerM, *localPointerDSG;

    if ((inpMsg->flag & ATOMIC_WRITE) && (inpMsg->size < MAX_DATA_IN_MSG)) {
        // Atomic write and data is in the rpc message
        try {
            // Allocate data item to hold the source data; update the
            // information in the message
            offsetDSG = allocator->allocate(ATOMIC_REGION_ID, inpMsg->size);
            inpMsg->offsetBuffer = offsetDSG;
            localPointerDSG =
                allocator->get_local_pointer(ATOMIC_REGION_ID, offsetDSG);
            memcpy(localPointerDSG, inpDataSG, inpMsg->size);
            openfam_persist(localPointerDSG, inpMsg->size);
            free(inpDataSG);
        } catch (Memory_Service_Exception &e) {
            return PUSHERROR;
        }
    }

    struct qData *lcqData;
    lcqData = (struct qData *)pointerQ;
    uint64_t offsetM;
    pthread_mutex_lock(&pushQmutex[qId]);
    // Get the offset of the element at the rear
    offsetM = *((uint64_t *)pointerA + lcqData->rear);
    // Get the pointer and copy the incoming message
    if ((qSize = lcqData->size.load(std::memory_order_acq_rel)) >=
        lcqData->capacity) {
        message << "Atomic Queue " << qId << " Full";
        pthread_mutex_unlock(&pushQmutex[qId]);
        return ATL_QUEUE_FULL;
    }
    // Increment the rear and size
    lcqData->rear = (lcqData->rear + 1) % lcqData->capacity;
    lcqData->size.fetch_add(1, std::memory_order_acq_rel);
    pthread_mutex_unlock(&pushQmutex[qId]);
    try {
        openfam_persist(pointerQ, sizeof(qData));
        localPointerM = allocator->get_local_pointer(ATOMIC_REGION_ID, offsetM);
        memcpy(localPointerM, inpMsg, sizeof(atomicMsg));
        openfam_persist(localPointerM, sizeof(atomicMsg));
    } catch (...) {
        return PUSHERROR;
    }
    if (qSize == 0)
        pthread_cond_signal(&empty[qId]); // Signal to processing thread that
                                          // queue is not empty anymore
    return 0;
}

/* Pop element from the queue using front pointer
 * @param item - Fam_Global_Descriptor
 * @return - {success(0), failure(1),ATOMIC_QUEUE_EMPTY
 */
int atomicQueue::read(Fam_Global_Descriptor *item) {
    struct qData *lcqData;
    lcqData = (struct qData *)pointerQ;
    if (lcqData->size.load(std::memory_order_acq_rel) == 0)
        return ATOMIC_QUEUE_EMPTY; // array empty
    item->regionId = ATOMIC_REGION_ID;
    // front should hold the offset of the data to be read
    item->offset = *((uint64_t *)pointerA + lcqData->front);
    return 0;
}

/* Remove the element from queue which was poped
 * @param item - Fam_Global_Descriptor
 * @return - {success(0), failure(1)}
 */
int atomicQueue::pop(Fam_Global_Descriptor *item) {
    void *localPointerM;
    struct qData *lcqData;
    lcqData = (struct qData *)pointerQ;
    // Adjust the front and size
    lcqData->front = (lcqData->front + 1) % lcqData->capacity;
    lcqData->size.fetch_add(-1, std::memory_order_acq_rel);
    try {
        openfam_persist(pointerQ, sizeof(qData));
        localPointerM =
            allocator->get_local_pointer(item->regionId, item->offset);
        // clear the content of the message
        memset(localPointerM, 0, sizeof(atomicMsg));
        openfam_persist(localPointerM, sizeof(atomicMsg));
    } catch (...) {
        return POPERROR;
    }
    return 0;
}

/* Check whether the queue is empty
 * return true(1), false(0)
 */
bool atomicQueue::isQempty() {
    struct qData *lcqData;
    lcqData = (struct qData *)pointerQ;
    return ((lcqData->size == 0) ? true : false);
}

/* Recover the incomplete transactions during startup
 * @param qId - queue number
 * @param - Memserver_Allocator
 */
int recover_queue(uint32_t qId, Memserver_Allocator *allocator) {
    Fam_Global_Descriptor item;
    void *localPointer = NULL, *localPointerB = NULL, *localPointerD = NULL;
    atomicMsg *msgPointer;
    int ret = 0;
    int retryCount = 0;
    while (!atomicQ[qId].isQempty()) {
        atomicQ[qId].read(&item);
        ret = 0;
        try {
            localPointer =
                allocator->get_local_pointer(item.regionId, item.offset);
        } catch (...) {
            ret = MAPERROR;
        }
        msgPointer = (atomicMsg *)localPointer;
        // Recover only Write and In progress requests
        if ((msgPointer->flag & ATOMIC_WRITE) &&
            (msgPointer->flag & ATOMIC_WRITE_IN_PROGRESS)) {
            // Atomic write and write was incomplete
            // get the pointer for target and source data items
            try {
                localPointerD = allocator->get_local_pointer(
                    msgPointer->dstDataGdesc.regionId,
                    msgPointer->dstDataGdesc.offset + msgPointer->offset);
                localPointerB = allocator->get_local_pointer(
                    ATOMIC_REGION_ID, msgPointer->offsetBuffer);
            } catch (...) {
                ret = MAPERROR;
            }

            // Copy data from source to destination data item
            memcpy(localPointerD, localPointerB, msgPointer->size);
            try {
                openfam_persist(localPointerD, msgPointer->size);
                // Clear the flags to indicate write is complete
                msgPointer->flag |= ATOMIC_WRITE_COMPLETED;
                msgPointer->flag &= ~ATOMIC_WRITE_IN_PROGRESS;
                openfam_persist(msgPointer, sizeof(msgPointer->flag));
            } catch (...) {
                ret = PERSISTERROR;
            }
            // Deallocate source data item
            try {
                allocator->deallocate(ATOMIC_REGION_ID,
                                      msgPointer->offsetBuffer);
            } catch (...) {
                ret = DEALLOCATEERROR;
            }
        }
        // Remove the message from the queue
        if (ret) {
            retryCount++;
            if (retryCount < 5)
                continue;
            else {
                break;
            }
        }
        atomicQ[qId].pop(&item);
    }
    return ret;
}

/* Main processing thread, one for each queue
 * Pops messages from queue and processes
 * Memory server call this with the argument list
 */
void *process_queue(void *arg) {
    tInfo *lcTInfo = (tInfo *)arg;
    Fam_Global_Descriptor item;
    void *localPointer = NULL, *localPointerB = NULL, *localPointerD = NULL,
         *localPointerInpD = NULL;
    atomicMsg *msgPointer;
    int ret = 0;
    int32_t retStatus = 0, popStatus = 0;
    Memserver_Allocator *allocator = lcTInfo->allocator;
    fi_addr_t fiAddr = 0, clientAddr;
    char *remoteAddr;
    std::map<fi_addr_t, fi_addr_t> fiAddrMap;
    std::map<fi_addr_t, fi_addr_t>::iterator it;

    // Recover the incomplete writes if any
    cout << "Recovering Incomplete transactions for queue" << lcTInfo->qId
         << endl;
    ret = recover_queue(lcTInfo->qId, allocator);
    if (ret) {
        // Recovery failed - Disable ATL
        cout << "Recovery error - Couldn't recover incomplete requests" << endl;
        numAtomicThreads = 0;
        pthread_exit(&ret);
    }
    cout << "Recovery of Incomplete trasactions completed for queue"
         << lcTInfo->qId << endl;
    cout << "processing thread for queue" << lcTInfo->qId << " started" << endl;

    while (1) {
        ret = atomicQ[lcTInfo->qId].read(&item);
        if (ret == ATOMIC_QUEUE_EMPTY) { // queue empty
            pthread_mutex_lock(&mutex[lcTInfo->qId]);
            pthread_cond_wait(&empty[lcTInfo->qId], &mutex[lcTInfo->qId]);
            pthread_mutex_unlock(&mutex[lcTInfo->qId]);
            continue;
        }
        retStatus = 0;
        popStatus = 0;
        // get fiAddr of the remote/client node
        try {
            localPointer =
                allocator->get_local_pointer(item.regionId, item.offset);
        } catch (...) {
            ret = MAPERROR;
        }
        msgPointer = (atomicMsg *)localPointer;
        remoteAddr = (char *)calloc(1, msgPointer->nodeAddrSize);
        memcpy(remoteAddr, msgPointer->nodeAddr, msgPointer->nodeAddrSize);
        clientAddr = *(fi_addr_t *)remoteAddr;
        it = fiAddrMap.find(clientAddr);
        if (it == fiAddrMap.end()) {
            try {
                std::vector<fi_addr_t> fiAddrV;
                ret = fabric_insert_av(remoteAddr, famOpsLibfabricQ->get_av(),
                                       &fiAddrV);
                fiAddr = fiAddrV[0];
                fiAddrMap.insert(
                    it, std::pair<fi_addr_t, fi_addr_t>(clientAddr, fiAddr));
            } catch (...) {
                // If error pop the item and continue;
                ret = AVINSERTERROR;
                cout << "AV insert error, Remore Address " << remoteAddr
                     << endl;
                ret = atomicQ[lcTInfo->qId].pop(&item);
                continue;
            }
        } else
            fiAddr = it->second;
        uint64_t function = msgPointer->flag & 0x3F;
        switch (function) {
        case ATOMIC_READ: { // Read function
            try {
                // Get the pointer for the source using the offset
                localPointerD = allocator->get_local_pointer(
                    msgPointer->dstDataGdesc.regionId,
                    msgPointer->dstDataGdesc.offset + msgPointer->offset);
                try {
                    // Write data to client's memory
                    ret = fabric_write(
                        msgPointer->key, localPointerD, msgPointer->size, 0,
                        fiAddr, famOpsLibfabricQ->get_defaultCtx(uint64_t(0)));
                } catch (...) {
                    retStatus = FABRICWRITEERROR;
                }
                // send status (retStatus)  back to client
                try {
                    fabric_send_response(
                        &retStatus, fiAddr,
                        famOpsLibfabricQ->get_defaultCtx(uint64_t(0)),
                        sizeof(retStatus));
                } catch (...) {
                    ret = SENDTOCLIENTERROR;
                }
            } catch (...) {
                ret = MAPERROR;
            }

            break;
        }
        case ATOMIC_WRITE: {
            // Atomic write
            if (msgPointer->flag & ATOMIC_WRITE_COMPLETED) {
                // last write completed. If buffer is still allocated,
                // deallocate and remove the entry
                if (msgPointer->flag & ATOMIC_BUFFER_ALLOCATED)
                    try {
                        allocator->deallocate(ATOMIC_REGION_ID,
                                              msgPointer->offsetBuffer);
                    } catch (...) {
                        popStatus = DEALLOCATEERROR;
                    }
                break;
            }

            if (msgPointer->flag & ATOMIC_CONTAIN_DATA) {
                // msg contains data
                try {
                    // Get the pointer to destination and source
                    localPointerD = allocator->get_local_pointer(
                        msgPointer->dstDataGdesc.regionId,
                        msgPointer->dstDataGdesc.offset + msgPointer->offset);
                    localPointerInpD = allocator->get_local_pointer(
                        ATOMIC_REGION_ID, msgPointer->offsetBuffer);
                    // Indicate write in progress
                    msgPointer->flag |= ATOMIC_WRITE_IN_PROGRESS;
                    // Copy the data from source to target
                    memcpy(localPointerD, localPointerInpD, msgPointer->size);
                    try {
                        openfam_persist(localPointerD, msgPointer->size);
                        // Set the flags
                        msgPointer->flag |= ATOMIC_WRITE_COMPLETED;
                        msgPointer->flag &= ~ATOMIC_WRITE_IN_PROGRESS;
                        openfam_persist(msgPointer, sizeof(msgPointer->flag));
                        // Deallocate source data item
                        try {
                            allocator->deallocate(ATOMIC_REGION_ID,
                                                  msgPointer->offsetBuffer);
                        } catch (...) {
                            popStatus = DEALLOCATEERROR;
                        }
                    } catch (...) {
                        popStatus = PERSISTERROR;
                    }
                } catch (...) {
                    popStatus = MAPERROR;
                }

                break;
            }
            if (msgPointer->flag & ATOMIC_WRITE_IN_PROGRESS) {
                // Write was incomplete. process similar to recovery operation
                try {
                    localPointerB = allocator->get_local_pointer(
                        ATOMIC_REGION_ID, msgPointer->offsetBuffer);
                } catch (...) {
                    popStatus = MAPERROR;
                }
            } else {
                // New write operation
                // Allocate data item for buffer
                uint64_t offsetB = 0;
                try {
                    offsetB =
                        allocator->allocate(ATOMIC_REGION_ID, msgPointer->size);
                    msgPointer->flag |= ATOMIC_BUFFER_ALLOCATED;
                    // Update the message with the region and offset of buffer
                    msgPointer->offsetBuffer = offsetB;
                    try {
                        localPointerB = allocator->get_local_pointer(
                            ATOMIC_REGION_ID, offsetB);
                        // Read from Client's memory into buffer
                        try {
                            ret = fabric_read(
                                msgPointer->key, localPointerB,
                                msgPointer->size, 0, fiAddr,
                                famOpsLibfabricQ->get_defaultCtx(uint64_t(0)));
                            try {
                                openfam_persist(localPointerB,
                                                msgPointer->size);
                                // Set the flag to indicate write is in progress
                                msgPointer->flag |= ATOMIC_WRITE_IN_PROGRESS;
                                openfam_persist(msgPointer,
                                                sizeof(msgPointer->flag));
                            } catch (...) {
                                retStatus = PERSISTERROR;
                            }
                        } catch (...) {
                            retStatus = FABRICREADERROR;
                        }
                    } catch (...) {
                        retStatus = MAPERROR;
                    }
                } catch (Memory_Service_Exception &e) {
                    retStatus = BUFFERALLOCATEERROR;
                }
                // Data is copied to buffer, now send the status back to client
                try {
                    fabric_send_response(
                        &retStatus, fiAddr,
                        famOpsLibfabricQ->get_defaultCtx(uint64_t(0)),
                        sizeof(retStatus));
                } catch (...) {
                    ret = SENDTOCLIENTERROR;
                }
            }
            // Get the pointer of the target and copy
            try {
                localPointerD = allocator->get_local_pointer(
                    msgPointer->dstDataGdesc.regionId,
                    msgPointer->dstDataGdesc.offset + msgPointer->offset);
            } catch (...) {
                popStatus = MAPERROR;
            }

            memcpy(localPointerD, localPointerB, msgPointer->size);
            // Update the flag to indicate write is completed
            msgPointer->flag |= ATOMIC_WRITE_COMPLETED;
            msgPointer->flag &= ~ATOMIC_WRITE_IN_PROGRESS;
            try {
                openfam_persist(msgPointer, sizeof(msgPointer->flag));
                openfam_persist(localPointerD, msgPointer->size);
            } catch (...) {
                popStatus = PERSISTERROR;
            }

            // Deallocate the buffer
            try {
                allocator->deallocate(ATOMIC_REGION_ID,
                                      msgPointer->offsetBuffer);
            } catch (...) {
                popStatus = DEALLOCATEERROR;
            }

            msgPointer->flag &= ~ATOMIC_BUFFER_ALLOCATED;

            break;
        }
        default:
            break;
        }
        // Remove the element from the queue
        if (remoteAddr)
            free(remoteAddr);
        // Pop the requests which are processed successfully (popStatus = 0)
        if (popStatus == 0)
            ret = atomicQ[lcTInfo->qId].pop(&item);
    }
    return 0;
}

} // namespace openfam

/*
 * atomic_queue.h
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

#ifndef ATOMIC_QUEUE_H_
#define ATOMIC_QUEUE_H_
#include "fam_ops_libfabric.h"
#include <allocator/memserver_allocator.h>
#include <atomic>
#include <fam/fam.h>
#include <fam/fam_exception.h>

#define MAX_NODE_ADDR_SIZE 64 //#define FT_MAX_CTRL_MSG 64
#define MAX_DATA_IN_MSG 128
#define ANONYMOUS_UNION_SIZE 32
#define ATOMIC_REGION "ATOMIC_REGION"
#define ATOMIC_REGION_ID 17
#define MAX_ATOMIC_THREADS 256
using namespace std;
namespace openfam {
// flag in atomicMsg structure;
// Keeps track of progress and request type

enum flag {
    ATOMIC_READ = 1,
    ATOMIC_WRITE = 2,
    ATOMIC_SCATTER_STRIDE = 4,
    ATOMIC_SCATTER_INDEX = 8,
    ATOMIC_GATHER_STRIDE = 16,
    ATOMIC_GATHER_INDEX = 32,
    ATOMIC_WRITE_IN_PROGRESS = 64,
    ATOMIC_WRITE_COMPLETED = 128,
    ATOMIC_BUFFER_ALLOCATED = 256,
    ATOMIC_CONTAIN_DATA = 512,
};
/*
 * Atomic request structure
 * flag: records progress and request type
 * nodeAddr: client Node address
 * nodeAddrSize: client Node address size
 * dstDataGdesc: target data item global descriptor
 * key: Key of the client memory to read/write
 * offsetBuffer: offset of the buffer data item
 * offset: offset to target data item for get/put
 * size: size of target data item for get/put
 * snElements: number of elements for strided scatter/gather
 * firstElement: first element for strided scatter/gather
 * stride: stride for strided scatter/gather
 * selementSize: size of element for strided scatter/gather
 * offsetIndex: offset of the index data item for indexed scatter/gather
 * inElements: index elements for indexed scatter/gather
 * ielementSize: size of element for indexed scatter/gather
 */
typedef struct atomicMsg {
    short int flag;
    char nodeAddr[MAX_NODE_ADDR_SIZE];
    uint32_t nodeAddrSize;
    Fam_Global_Descriptor dstDataGdesc;
    uint64_t key;
    uint64_t srcBaseAddr;
    uint64_t offsetBuffer;
    union {
        struct {
            uint64_t offset;
            uint64_t size;
        };
        struct {
            uint64_t snElements;
            uint64_t firstElement;
            uint64_t stride;
            uint64_t selementSize;
        };
        struct {
            uint64_t offsetIndex;
            uint64_t inElements;
            uint64_t ielementSize;
        };
    };
} atomicMsg;

typedef struct threadInfo {
    Memserver_Allocator *allocator;
    uint32_t qId;
} tInfo;

struct qData {
    int64_t front;
    int64_t rear;
    uint32_t capacity;
    atomic_uint size;
    uint64_t offsetArray;
};
class atomicQueue {
  private:
    uint32_t qId;
    Memserver_Allocator *allocator;
    void *pointerA, *pointerQ;

  public:
    atomicQueue() {}
    int create(Memserver_Allocator *inp_allocator, const uint32_t in_qid);
    int push(atomicMsg *item, const void *inpDataSG);
    int read(Fam_Global_Descriptor *gitem);
    int pop(Fam_Global_Descriptor *item);
    bool isQempty();
};

extern int numAtomicThreads;
extern uint64_t memoryPerThread;
extern int queueCapacity;
extern pthread_t atid[MAX_ATOMIC_THREADS];
extern atomicQueue atomicQ[MAX_ATOMIC_THREADS];
extern tInfo atomicTInfo[MAX_ATOMIC_THREADS];
extern Fam_Ops_Libfabric *famOpsLibfabricQ;
void *process_queue(void *);
extern void *atomicRegionIdRoot;
extern pthread_rwlock_t fiAddrLock;

enum ATLError {
    ATOMIC_QUEUE_EMPTY = 100,
    ALLOCATEERROR,
    MAPERROR,
    PERSISTERROR,
    PUSHERROR,
    POPERROR,
    DEALLOCATEERROR,
    AVINSERTERROR,
    FABRICWRITEERROR,
    SENDTOCLIENTERROR,
    BUFFERALLOCATEERROR,
    FABRICREADERROR
};
} // namespace openfam
#endif

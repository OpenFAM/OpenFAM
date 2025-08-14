/*
 * fam_microbenchmark_datapath2.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
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

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fam/fam_exception.h>
#include <gtest/gtest.h>
#include <iostream>
#include <random>

#include <assert.h>
#include <math.h>
#include <numa.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "cis/fam_cis_client.h"
#include "cis/fam_cis_direct.h"
#include "common/fam_libfabric.h"
#include <fam/fam.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

#define BIG_REGION_SIZE 4294967296ULL

Fam_CIS *cis;
fam *my_fam;
Fam_Options fam_opts;
Fam_Descriptor *item;
Fam_Region_Descriptor *desc;
mode_t test_perm_mode;
size_t test_item_size;

void *gLocalBuf = NULL;
uint64_t gItemSize = (1 * 1024 * 1024 * 1024ULL);

uint64_t gDataSize = 256;
uint64_t numThreads = 2;

uint64_t numLocalBufs = 10000;

fam_context **ctx;

#define LOCAL_BUFFER_SIZE 2 * 1024 * 1024UL // 2MB

struct ValueInfo {
    Fam_Descriptor *item;
    uint64_t tid;
    uint64_t num_ops;
    void *localBuf;
};

void pinThreadToCore(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    //CPU_SET(((core_id % 4) * 16) + (core_id / 4), &cpuset);

    int result =
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
        fprintf(stderr, "Error setting thread affinity: %d\n", result);
    }
}

void *func_non_blocking_get_single_local_buf(void *arg) {
    ValueInfo *it = (ValueInfo *)arg;
    pinThreadToCore((int)it->tid);

    uint64_t offset = 0;
    void *LocalBuf = it->localBuf;

    for (uint64_t i = 0; i < it->num_ops; i++) {
        ctx[it->tid]->fam_get_nonblocking(LocalBuf, it->item, offset,
                                          gDataSize);
    }
    ctx[it->tid]->fam_quiet();

    pthread_exit(NULL);
}

void *func_non_blocking_get_multiple_local_bufs(void *arg) {
    ValueInfo *it = (ValueInfo *)arg;
    pinThreadToCore((int)it->tid);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (int)numLocalBufs - 1);

    uint64_t offset = 0;
    void *LocalBuf = nullptr;
    size_t idx = 0;

    for (uint64_t i = 0; i < it->num_ops; i++) {
        idx = dis(gen);
        LocalBuf = (void *)(((uint64_t *)it->localBuf)[idx]);
        ctx[it->tid]->fam_get_nonblocking(LocalBuf, it->item, offset,
                                          gDataSize);
    }
    ctx[it->tid]->fam_quiet();

    pthread_exit(NULL);
}

// Test case 1 -  NonBlocking get test, a shared single context, a buffer per
// thread
TEST(FamNonBlockingGet, NonBlockingFamGetSingleContext) {
    uint64_t num_ios = gItemSize / gDataSize;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * numThreads);
    int rc;

    Fam_Region_Descriptor **descs = (Fam_Region_Descriptor **)malloc(
        sizeof(Fam_Region_Descriptor *) * numThreads);
    Fam_Descriptor **items =
        (Fam_Descriptor **)malloc(sizeof(Fam_Descriptor *) * numThreads);

    ValueInfo *infos = new ValueInfo[numThreads];

    std::string regionPrefix("region");
    std::string itemPrefix("item");
    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        regionPrefix.c_str(), BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(itemPrefix.c_str(), gItemSize, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    ctx = (fam_context **)malloc(sizeof(fam_context *) * numThreads);
    for (uint64_t i = 0; i < numThreads; i++) {
        if (i == 0) {
            ctx[i] = my_fam->fam_context_open();
        } else {
            ctx[i] = ctx[0];
        }
    }

    void **newLocalBufs = (void **)malloc(sizeof(void *) * numThreads);
    for (uint64_t i = 0; i < numThreads; i++) {
        newLocalBufs[i] = malloc(LOCAL_BUFFER_SIZE);
        memset(newLocalBufs[i], 0, LOCAL_BUFFER_SIZE);
        my_fam->fam_register_local_buffer(newLocalBufs[i], LOCAL_BUFFER_SIZE);
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        items[i] = item;
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        infos[i].item = items[i];
        infos[i].tid = i;
        infos[i].num_ops = num_ios / numThreads;
        infos[i].localBuf = newLocalBufs[i];
    }

    auto starttime = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < numThreads; i++) {
        if ((rc = pthread_create(&threads[i], NULL,
                                 func_non_blocking_get_single_local_buf,
                                 &infos[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - starttime);
    printf("Elapsed time (ms): %.f ms\n", (double)duration.count() / 1000.0);
    printf("Throughput: %f GB/sec\n",
           ((double)gItemSize / (1024 * 1024 * 1024)) /
               ((double)duration.count() / 1000000.0));

    // Deallocating data items
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(items);

    // Destroying the region
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    free(descs);

    my_fam->fam_context_close(ctx[0]);
    free(ctx);

    for (uint64_t i = 0; i < numThreads; i++) {
        free(newLocalBufs[i]);
    }
    free(newLocalBufs);

    delete[] infos;
    free(threads);
}

// Test case 2 -  NonBlocking get test, multiple contexts, a context, buffer per
// thread
TEST(FamNonBlockingGet, NonBlockingFamGetMultiContexts) {
    uint64_t num_ios = gItemSize / gDataSize;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * numThreads);
    int rc;

    Fam_Region_Descriptor **descs = (Fam_Region_Descriptor **)malloc(
        sizeof(Fam_Region_Descriptor *) * numThreads);
    Fam_Descriptor **items =
        (Fam_Descriptor **)malloc(sizeof(Fam_Descriptor *) * numThreads);

    ValueInfo *infos = new ValueInfo[numThreads];

    std::string regionPrefix("region");
    std::string itemPrefix("item");
    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        regionPrefix.c_str(), BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(itemPrefix.c_str(), gItemSize, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    ctx = (fam_context **)malloc(sizeof(fam_context *) * numThreads);
    for (uint64_t i = 0; i < numThreads; i++) {
        ctx[i] = my_fam->fam_context_open();
    }

    void **newLocalBufs = (void **)malloc(sizeof(void *) * numThreads);
    for (uint64_t i = 0; i < numThreads; i++) {
        newLocalBufs[i] = malloc(LOCAL_BUFFER_SIZE);
        memset(newLocalBufs[i], 0, LOCAL_BUFFER_SIZE);
        my_fam->fam_register_local_buffer(newLocalBufs[i], LOCAL_BUFFER_SIZE);
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        items[i] = item;
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        infos[i].item = items[i];
        infos[i].tid = i;
        infos[i].num_ops = num_ios / numThreads;
        infos[i].localBuf = newLocalBufs[i];
    }

    auto starttime = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < numThreads; i++) {
        if ((rc = pthread_create(&threads[i], NULL,
                                 func_non_blocking_get_single_local_buf,
                                 &infos[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - starttime);
    printf("Elapsed time (ms): %.f ms\n", (double)duration.count() / 1000.0);
    printf("Throughput: %f GB/sec\n",
           ((double)gItemSize / (1024 * 1024 * 1024)) /
               ((double)duration.count() / 1000000.0));

    // Deallocating data items
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(items);

    // Destroying the region
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    free(descs);

    for (uint64_t i = 0; i < numThreads; i++) {
        my_fam->fam_context_close(ctx[i]);
    }
    free(ctx);

    for (uint64_t i = 0; i < numThreads; i++) {
        free(newLocalBufs[i]);
    }
    free(newLocalBufs);

    delete[] infos;
    free(threads);
}

// Test case 3 -  NonBlocking get test, multiple contexts, a context per thread,
// multiple shared buffers
TEST(FamNonBlockingGet, NonBlockingFamGetMultiContextsBuffers) {
    uint64_t num_ios = gItemSize / gDataSize;
    pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * numThreads);
    int rc;

    Fam_Region_Descriptor **descs = (Fam_Region_Descriptor **)malloc(
        sizeof(Fam_Region_Descriptor *) * numThreads);
    Fam_Descriptor **items =
        (Fam_Descriptor **)malloc(sizeof(Fam_Descriptor *) * numThreads);

    ValueInfo *infos = new ValueInfo[numThreads];

    std::string regionPrefix("region");
    std::string itemPrefix("item");
    EXPECT_NO_THROW(desc = my_fam->fam_create_region(
                        regionPrefix.c_str(), BIG_REGION_SIZE, 0777, NULL));
    EXPECT_NE((void *)NULL, desc);

    EXPECT_NO_THROW(
        item = my_fam->fam_allocate(itemPrefix.c_str(), gItemSize, 0777, desc));
    EXPECT_NE((void *)NULL, item);

    ctx = (fam_context **)malloc(sizeof(fam_context *) * numThreads);
    for (uint64_t i = 0; i < numThreads; i++) {
        ctx[i] = my_fam->fam_context_open();
    }

    void **newLocalBufs = (void **)malloc(sizeof(void *) * numLocalBufs);
    void *sharedLocalBuf =
        malloc(LOCAL_BUFFER_SIZE * numLocalBufs); // 20MB shared buffer
    memset(sharedLocalBuf, 0, LOCAL_BUFFER_SIZE * numLocalBufs);
    for (uint64_t i = 0; i < numLocalBufs; i++) {
        newLocalBufs[i] =
            (void *)((char *)sharedLocalBuf + (i * LOCAL_BUFFER_SIZE));
        my_fam->fam_register_local_buffer(newLocalBufs[i], LOCAL_BUFFER_SIZE);
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        items[i] = item;
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        infos[i].item = items[i];
        infos[i].tid = i;
        infos[i].num_ops = num_ios / numThreads;
        infos[i].localBuf = newLocalBufs;
    }

    auto starttime = std::chrono::system_clock::now();
    for (uint64_t i = 0; i < numThreads; i++) {
        if ((rc = pthread_create(&threads[i], NULL,
                                 func_non_blocking_get_multiple_local_bufs,
                                 &infos[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            exit(1);
        }
    }

    for (uint64_t i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - starttime);
    printf("Elapsed time (ms): %.f ms\n", (double)duration.count() / 1000.0);
    printf("Throughput: %f GB/sec\n",
           ((double)gItemSize / (1024 * 1024 * 1024)) /
               ((double)duration.count() / 1000000.0));

    // Deallocating data items
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    free(items);

    // Destroying the region
    EXPECT_NO_THROW(my_fam->fam_destroy_region(desc));
    free(descs);

    for (uint64_t i = 0; i < numThreads; i++) {
        my_fam->fam_context_close(ctx[i]);
    }
    free(ctx);

    free(sharedLocalBuf);
    free(newLocalBufs);

    delete[] infos;
    free(threads);
}

int main(int argc, char **argv) {
    int ret;
    ::testing::InitGoogleTest(&argc, argv);

    if (argc == 3) {
        gDataSize = atoi(argv[1]);
        numThreads = atoi(argv[2]);
    }

    std::cout << "gDataSize = " << gDataSize << std::endl;
    std::cout << "numThreads = " << numThreads << std::endl;

    my_fam = new fam();

    init_fam_options(&fam_opts);

    gLocalBuf = (int64_t *)malloc(LOCAL_BUFFER_SIZE);
    memset(gLocalBuf, 2, LOCAL_BUFFER_SIZE);

    fam_opts.local_buf_addr = gLocalBuf;
    fam_opts.local_buf_size = LOCAL_BUFFER_SIZE;

    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    ret = RUN_ALL_TESTS();

    cout << "finished all testing" << endl;
    EXPECT_NO_THROW(my_fam->fam_finalize("default"));
    cout << "Finalize done : " << ret << endl;

    delete my_fam;
    free(gLocalBuf);

    return ret;
}

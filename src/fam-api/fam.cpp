/*
 * fam.cpp
 * Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following conditions
 * are met:
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
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

#include "allocator/fam_allocator_client.h"
#include "common/fam_libfabric.h"
#include "common/fam_ops.h"
#include "common/fam_ops_libfabric.h"
#include "common/fam_ops_shm.h"
#include "common/fam_options.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "pmi/fam_runtime.h"
#include "pmi/runtime_pmi2.h"
#include "pmi/runtime_pmix.h"
#ifdef FAM_PROFILE
#include "fam_counters.h"
#endif

using namespace std;

/**
 * List of Options supported by this OpenFAM implementation.
 * Defined as static list of option array.
 */
const char *supportedOptionList[] = {
    "VERSION",             // index #0
    "DEFAULT_REGION_NAME", // index #1
    "MEMORY_SERVER",       // index #2
    "GRPC_PORT",           // index #3
    "LIBFABRIC_PORT",      // index #4
    "LIBFABRIC_PROVIDER",  // index #5
    "FAM_THREAD_MODEL",    // index #6
    "ALLOCATOR",           // index #7
    "FAM_CONTEXT_MODEL",   // index #8
    "PE_COUNT",            // index #9
    "PE_ID",               // index #10
    "RUNTIME",             // index #11
    "NUM_CONSUMER",        // index #12
    NULL                   // index #13
};

namespace openfam {
/*
 * Internal implementation of fam
 */
class fam::Impl_ {
  public:
    Impl_() {
        uid = getuid();
        gid = getgid();

        optValueMap = NULL;
        groupName = NULL;
        famOps = NULL;
        famAllocator = NULL;
        famRuntime = NULL;
        memset((void *)&famOptions, 0, sizeof(Fam_Options));
    }

    ~Impl_() {
        if (groupName)
            free(groupName);
        if (famOps)
            delete (famOps);
        if (famAllocator)
            delete famAllocator;
        if (famRuntime)
            delete famRuntime;
    }

    int fam_initialize(const char *groupName, Fam_Options *options);

    void fam_finalize(const char *groupName);

    void fam_abort(int status);

    void fam_barrier_all(void);

    const char **fam_list_options(void);

    const void *fam_get_option(char *optionName);

    Fam_Region_Descriptor *fam_lookup_region(const char *name);

    Fam_Descriptor *fam_lookup(const char *itemName, const char *regionName);

    Fam_Region_Descriptor *
    fam_create_region(const char *name, uint64_t size, mode_t permissions,
                      Fam_Redundancy_Level redundancyLevel, ...);

    void fam_destroy_region(Fam_Region_Descriptor *descriptor);

    int fam_resize_region(Fam_Region_Descriptor *descriptor, uint64_t nbytes);

    Fam_Descriptor *fam_allocate(uint64_t nbytes, mode_t accessPermissions,
                                 Fam_Region_Descriptor *region);
    Fam_Descriptor *fam_allocate(const char *name, uint64_t nbytes,
                                 mode_t accessPermissions,
                                 Fam_Region_Descriptor *region);

    void fam_deallocate(Fam_Descriptor *descriptor);

    int fam_change_permissions(Fam_Descriptor *descriptor,
                               mode_t accessPermissions);

    int fam_change_permissions(Fam_Region_Descriptor *descriptor,
                               mode_t accessPermissions);

    void fam_stat(Fam_Descriptor *descriptor, Fam_Stat *famInfo);
    void fam_stat(Fam_Region_Descriptor *descriptor, Fam_Stat *famInfo);

    int fam_get_blocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t offset, uint64_t nbytes);

    void fam_get_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes);

    int fam_put_blocking(void *local, Fam_Descriptor *descriptor,
                         uint64_t offset, uint64_t nbytes);

    void fam_put_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes);

    void *fam_map(Fam_Descriptor *descriptor);

    void fam_unmap(void *local, Fam_Descriptor *descriptor);

    int fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                            uint64_t nElements, uint64_t firstElement,
                            uint64_t stride, uint64_t elementSize);

    int fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                            uint64_t nElements, uint64_t *elementIndex,
                            uint64_t elementSize);

    void fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t firstElement,
                                uint64_t stride, uint64_t elementSize);

    void fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t *elementIndex,
                                uint64_t elementSize);

    int fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t firstElement,
                             uint64_t stride, uint64_t elementSize);

    int fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t *elementIndex,
                             uint64_t elementSize);

    void fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t firstElement,
                                 uint64_t stride, uint64_t elementSize);

    void fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t *elementIndex,
                                 uint64_t elementSize);

    void *fam_copy(Fam_Descriptor *src, uint64_t srcOffset,
                   Fam_Descriptor *dest, uint64_t destOffset, uint64_t nbytes);

    void fam_copy_wait(void *waitObj);

    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int128_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      int32_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      int64_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      uint32_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      uint64_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      double value);

    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, double value);

    void fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    void fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    void fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    int32_t fam_fetch_int32(Fam_Descriptor *descriptor, uint64_t offset);
    int64_t fam_fetch_int64(Fam_Descriptor *descriptor, uint64_t offset);
    int128_t fam_fetch_int128(Fam_Descriptor *descriptor, uint64_t offset);
    uint32_t fam_fetch_uint32(Fam_Descriptor *descriptor, uint64_t offset);
    uint64_t fam_fetch_uint64(Fam_Descriptor *descriptor, uint64_t offset);
    float fam_fetch_float(Fam_Descriptor *descriptor, uint64_t offset);
    double fam_fetch_double(Fam_Descriptor *descriptor, uint64_t offset);

    int32_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                     int32_t value);
    int64_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                     int64_t value);
    uint32_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      uint32_t value);
    uint64_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      uint64_t value);
    float fam_swap(Fam_Descriptor *descriptor, uint64_t offset, float value);
    double fam_swap(Fam_Descriptor *descriptor, uint64_t offset, double value);

    int32_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t oldValue, int32_t newValue);
    int64_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t oldValue, int64_t newValue);
    uint32_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t oldValue, uint32_t newValue);
    uint64_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t oldValue, uint64_t newValue);
    int128_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              int128_t oldValue, int128_t newValue);

    int32_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

    int32_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                               int32_t value);
    int64_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                               int64_t value);
    uint32_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                uint32_t value);
    uint64_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                uint64_t value);
    float fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                             float value);
    double fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              double value);

    int32_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

    int32_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

    uint32_t fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);

    uint32_t fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                          uint32_t value);
    uint64_t fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                          uint64_t value);

    uint32_t fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);

    void fam_fence(Fam_Region_Descriptor *descriptor = NULL);
    void fam_quiet(Fam_Region_Descriptor *descriptor = NULL);

    int validate_fam_options(Fam_Options *options);
    void clean_fam_options();
    int validate_item(Fam_Descriptor *descriptor);

  private:
    uid_t uid;
    gid_t gid;

    char *groupName;
    Fam_Options famOptions;
    std::map<std::string, const void *> *optValueMap;
    Fam_Ops *famOps;
    Fam_Allocator_Client *famAllocator;
    Fam_Thread_Model famThreadModel;
    Fam_Context_Model famContextModel;
    Fam_Runtime *famRuntime;
    uint64_t memoryServerCount;
    uint64_t generate_memory_server_id(const char *name) {
        std::uint64_t hashVal = std::hash<std::string>{}(name);
        return hashVal % memoryServerCount;
    }

#ifdef FAM_PROFILE
    Fam_Counter_St profileData[fam_counter_max][FAM_CNTR_TYPE_MAX];
    uint64_t profile_time;
    uint64_t profile_start;
#define OUTPUT_WIDTH 120
#define ITEM_WIDTH OUTPUT_WIDTH / 5
    uint64_t fam_get_time() {
#if 1
        long int time = static_cast<long int>(
            duration_cast<nanoseconds>(
                high_resolution_clock::now().time_since_epoch())
                .count());
        return time;
#else // using intel tsc
        uint64_t hi, lo, aux;
        __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux));
        return (uint64_t)lo | ((uint64_t)hi << 32);
#endif
    }

    uint64_t fam_time_diff_nanoseconds(Fam_Profile_Time start,
                                       Fam_Profile_Time end) {
        return (end - start);
    }
#define FAM_PROFILE_START_TIME() profile_start = fam_get_time();
#define FAM_PROFILE_INIT() fam_profile_init();
#define FAM_PROFILE_END()                                                      \
    {                                                                          \
        profile_time = fam_get_time() - profile_start;                         \
        fam_dump_profile_banner();                                             \
        fam_dump_profile_data();                                               \
        fam_dump_profile_summary();                                            \
    }

#define FAM_CNTR_INC_API(apiIdx) __FAM_CNTR_INC_API(prof_##apiIdx)
#define FAM_PROFILE_START_ALLOCATOR(apiIdx) __FAM_PROFILE_START_ALLOCATOR()
#define FAM_PROFILE_END_ALLOCATOR(apiIdx)                                      \
    __FAM_PROFILE_END_ALLOCATOR(prof_##apiIdx)
#define FAM_PROFILE_START_OPS(apiIdx) __FAM_PROFILE_START_OPS()
#define FAM_PROFILE_END_OPS(apiIdx) __FAM_PROFILE_END_OPS(prof_##apiIdx)

#define __FAM_CNTR_INC_API(apiIdx)                                             \
    uint64_t one = 1;                                                          \
    profileData[apiIdx][FAM_CNTR_API].count.fetch_add(                         \
        one, boost::memory_order_seq_cst);
#define __FAM_PROFILE_START_ALLOCATOR(apiIdx)                                  \
    Fam_Profile_Time startAlloc = fam_get_time();

#define __FAM_PROFILE_START_OPS(apiIdx)                                        \
    Fam_Profile_Time startOps = fam_get_time();

#define __FAM_PROFILE_END_ALLOCATOR(apiIdx)                                    \
    Fam_Profile_Time endAlloc = fam_get_time();                                \
    Fam_Profile_Time totalAlloc =                                              \
        fam_time_diff_nanoseconds(startAlloc, endAlloc);                       \
    fam_add_to_total_profile(FAM_CNTR_ALLOCATOR, apiIdx, totalAlloc);

#define __FAM_PROFILE_END_OPS(apiIdx)                                          \
    Fam_Profile_Time endOps = fam_get_time();                                  \
    Fam_Profile_Time totalOps = fam_time_diff_nanoseconds(startOps, endOps);   \
    fam_add_to_total_profile(FAM_CNTR_OPS, apiIdx, totalOps);

    void fam_profile_init() { memset(profileData, 0, sizeof(profileData)); }

    void fam_add_to_total_profile(Fam_Counter_Type_T type, int apiIdx,
                                  Fam_Profile_Time total) {
        profileData[apiIdx][type].total.fetch_add(total,
                                                  boost::memory_order_seq_cst);
    }

    void fam_total_api_time(int apiIdx) {
        uint64_t total = profileData[apiIdx][FAM_CNTR_ALLOCATOR].total.load(
                             boost::memory_order_seq_cst) +
                         profileData[apiIdx][FAM_CNTR_OPS].total.load(
                             boost::memory_order_seq_cst);
        profileData[apiIdx][FAM_CNTR_API].total.fetch_add(
            total, boost::memory_order_seq_cst);
    }

    void fam_dump_profile_banner(void) {
        {
            string header = "FAM PROFILE DATA";
            cout << endl;
            cout << setfill('-') << setw(OUTPUT_WIDTH) << "-" << endl;
            cout << setfill(' ')
                 << setw((int)(OUTPUT_WIDTH - header.length()) / 2) << " ";
            cout << "FAM PROFILE DATA";
            cout << setfill(' ')
                 << setw((int)(OUTPUT_WIDTH - header.length()) / 2) << " "
                 << endl;
            cout << setfill('-') << setw(OUTPUT_WIDTH) << "-" << endl;
        }
#define DUMP_HEADING1(name)                                                    \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << name;
        DUMP_HEADING1("Function");
        DUMP_HEADING1("Count");
        DUMP_HEADING1("Total Pct");
        DUMP_HEADING1("Total time(ns)");
        DUMP_HEADING1("Avg time/call(ns)");
        cout << endl;
#define DUMP_HEADING2(name)                                                    \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH)                      \
         << string(strlen(name), '-');
        DUMP_HEADING2("Function");
        DUMP_HEADING2("Count");
        DUMP_HEADING2("Total Pct");
        DUMP_HEADING2("Total time(ns)");
        DUMP_HEADING2("Avg time/call(ns)");
        cout << endl;
    }

    void fam_dump_profile_data(void) {
#define DUMP_DATA_COUNT(type, idx)                                             \
    cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)       \
         << profileData[idx][type].count;

#define DUMP_DATA_TIME(type, idx)                                              \
    cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)       \
         << profileData[idx][type].total;

#define DUMP_DATA_PCT(type, idx)                                               \
    {                                                                          \
        double time_pct = (double)((double)profileData[idx][type].total *      \
                                   100 / (double)profile_time);                \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << std::fixed    \
             << setprecision(2) << time_pct;                                   \
    }

#define DUMP_DATA_AVG(type, idx)                                               \
    {                                                                          \
        uint64_t avg_time =                                                    \
            (profileData[idx][type].total / profileData[idx][type].count);     \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << avg_time;     \
    }

#undef FAM_COUNTER
#define FAM_TOTAL_API_TIME(apiIdx) fam_total_api_time(apiIdx);
#define FAM_COUNTER(apiIdx) FAM_TOTAL_API_TIME(prof_##apiIdx)
#include "fam_counters.tbl"
#undef FAM_COUNTER
#define FAM_COUNTER(name) __FAM_COUNTER(name, prof_##name)
#define __FAM_COUNTER(name, apiIdx)                                            \
    if (profileData[apiIdx][FAM_CNTR_API].count) {                             \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << #name;        \
        DUMP_DATA_COUNT(FAM_CNTR_API, apiIdx);                                 \
        DUMP_DATA_PCT(FAM_CNTR_API, apiIdx);                                   \
        DUMP_DATA_TIME(FAM_CNTR_API, apiIdx);                                  \
        DUMP_DATA_AVG(FAM_CNTR_API, apiIdx);                                   \
        cout << endl;                                                          \
    }
#include "fam_counters.tbl"
        cout << endl;
    }
    void fam_dump_profile_summary(void) {
        uint64_t fam_lib_time = 0;
        uint64_t fam_alloc_time = 0;
        uint64_t fam_ops_time = 0;
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << "Summary"
             << endl;
        ;
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH)
             << string(strlen("Summary"), '-') << endl;

#define FAM_SUMMARY_ENTRY(name, value)                                         \
    cout << std::left << std::fixed << setprecision(2) << setfill(' ')         \
         << setw(ITEM_WIDTH) << name << setw(10) << ":" << value << " ns ("    \
         << value * 100 / profile_time << "%)" << endl;
#undef FAM_COUNTER
#undef __FAM_COUNTER
#define FAM_COUNTER(name) __FAM_COUNTER(prof_##name)
#define __FAM_COUNTER(apiIdx)                                                  \
    {                                                                          \
        fam_lib_time += profileData[apiIdx][FAM_CNTR_API].total;               \
        fam_alloc_time += profileData[apiIdx][FAM_CNTR_ALLOCATOR].total;       \
        fam_ops_time += profileData[apiIdx][FAM_CNTR_OPS].total;               \
    }
#include "fam_counters.tbl"

        FAM_SUMMARY_ENTRY("Total time", profile_time);
        FAM_SUMMARY_ENTRY("OpenFAM library", fam_lib_time);
        FAM_SUMMARY_ENTRY("Allocator", fam_alloc_time);
        FAM_SUMMARY_ENTRY("DataPath", fam_ops_time);
        cout << endl;
    }

#else
#define FAM_PROFILE_START_TIME()
#define FAM_PROFILE_INIT()
#define FAM_PROFILE_END()
#define FAM_CNTR_INC_API(apiIdx)
#define FAM_PROFILE_START_ALLOCATOR(apiIdx)
#define FAM_PROFILE_END_ALLOCATOR(apiIdx)
#define FAM_PROFILE_START_OPS(apiIdx)
#define FAM_PROFILE_END_OPS(apiIdx)
#endif
};

/**
 * fam() - constructor for fam class
 */
/**
 * Initialize the OpenFAM library.
 * @param groupName - name of the group of cooperating PEs.
 * @param options - options structure containing initialization choices
 * @return - {true(0), false(1), errNo(<0)}
 * @throws Fam_InvalidOption_Exception - for invalid option values
 * @throws Fam_Datapath_Exception - for fabric initialization failure
 * @throws Fam_Allocator_Exception - for allocator grpc initialization failure
 * @see #fam_finalize()
 * @see #Fam_Options
 */
int fam::Impl_::fam_initialize(const char *grpName, Fam_Options *options) {
    std::ostringstream message;
    int ret = 0;
    int *peCnt;
    int *peId;
    famRuntime = NULL;
    FAM_PROFILE_INIT();
    peCnt = (int *)malloc(sizeof(int));
    peId = (int *)malloc(sizeof(int));
    if (grpName)
        groupName = strdup(grpName);
    else {
        message << "Fam Invalid Option specified: GroupName";
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    // Initialize Options
    //
    optValueMap = new std::map<std::string, const void *>();

    optValueMap->insert({supportedOptionList[VERSION], strdup("0.0.1")});

    memoryServerCount = 1;

    if ((ret = validate_fam_options(options)) < 0) {
        return ret;
    }
    if (strcmp(famOptions.runtime, FAM_OPTIONS_RUNTIME_NONE_STR) == 0) {
        *peId = 0;
        *peCnt = 1;
        optValueMap->insert({supportedOptionList[PE_COUNT], peCnt});
        optValueMap->insert({supportedOptionList[PE_ID], peId});
    } else {
        // Initialize PMI Runtime
        if (strcmp(famOptions.runtime, FAM_OPTIONS_RUNTIME_PMI2_STR) == 0) {
            // initialize PMI2
            famRuntime = new Pmi2_Runtime();
            if (PMI2_SUCCESS != (ret = famRuntime->runtime_init())) {
                // Raising an exception will make the use of pmix mandatory.
                message << "Fam PMI2 Runtime initialization failed: " << ret;
                throw Fam_Pmi_Exception(message.str().c_str());
            }
        } else {
            // initialize PMIX
            famRuntime = new Pmix_Runtime();
            if (PMIX_SUCCESS != (ret = famRuntime->runtime_init())) {
                // Raising an exception will make the use of pmix mandatory.
                message << "Fam PMIx Runtime initialization failed: " << ret;
                throw Fam_Pmi_Exception(message.str().c_str());
            }
        }
        // Add PE_COUNT and PE_ID to optValueMap

        *peCnt = famRuntime->num_pes();
        if (*peCnt < 0) {
            message << "PMIx Runtime Failed to get PE_COUNT: " << *peCnt;
            free(peCnt);
            throw Fam_Exception(message.str().c_str());
        }

        *peId = famRuntime->my_pe();
        if (*peId < 0) {
            message << "PMIx Runtime Failed to get PE_ID: " << *peId;
            free(peCnt);
            free(peId);
            throw Fam_Exception(message.str().c_str());
        }

        optValueMap->insert({supportedOptionList[PE_COUNT], peCnt});
        optValueMap->insert({supportedOptionList[PE_ID], peId});
    }

    if (strcmp(famOptions.allocator, FAM_OPTIONS_SHM_STR) == 0) {
        // initialize NVMM client
        famAllocator = new Fam_Allocator_Client();
        famOps = new Fam_Ops_SHM(famThreadModel, famContextModel, famAllocator,
                                 atoi(famOptions.numConsumer));
        ret = famOps->initialize();
    } else {
        std::string memoryServer = famOptions.memoryServer;

        CISServerMap memoryServerList;
        std::string delimiter1 = ",";
        std::string delimiter2 = ":";

        memoryServerList =
            parse_memserver_list(memoryServer, delimiter1, delimiter2);

        if (memoryServerList.size() == 0) {
            throw Fam_InvalidOption_Exception("Memory server list not found");
        }

        memoryServerCount = memoryServerList.size();

        for (auto it = memoryServerList.begin(); it != memoryServerList.end();
             ++it) {
            if (it->first >= memoryServerCount) {
                message << "Fam Invalid memory server ID specified: "
                        << it->first
                        << " should be less than memory server count";
                throw Fam_InvalidOption_Exception(message.str().c_str());
            }
        }
        famAllocator = new Fam_Allocator_Client(memoryServerList,
                                                atoi(famOptions.grpcPort));
        famOps = new Fam_Ops_Libfabric(
            memoryServerList, famOptions.libfabricPort, false,
            famOptions.libfabricProvider, famThreadModel, famAllocator,
            famContextModel);

        ret = famOps->initialize();
        if (ret < 0) {
            message << "Fam libfabric initialization failed: "
                    << fabric_strerror(ret);
            if (famRuntime != NULL)
                famRuntime->runtime_fini();
            throw Fam_Datapath_Exception(message.str().c_str());
        }
    }
    FAM_PROFILE_START_TIME();
    return ret;
}

/**
 * Validate the input Fam_Options and update the option values
 * @return - {true(0), false(1), errNo(<0)}
 */
int fam::Impl_::validate_fam_options(Fam_Options *options) {
    int ret = 0;

    std::ostringstream message;
    if (options && options->defaultRegionName)
        famOptions.defaultRegionName = strdup(options->defaultRegionName);
    else
        famOptions.defaultRegionName = strdup("Default");

    optValueMap->insert({supportedOptionList[DEFAULT_REGION_NAME],
                         famOptions.defaultRegionName});

    if (options && options->memoryServer)
        famOptions.memoryServer = strdup(options->memoryServer);
    else
        famOptions.memoryServer = strdup("0:127.0.0.1");

    optValueMap->insert(
        {supportedOptionList[MEMORY_SERVER], famOptions.memoryServer});

    if (options && options->grpcPort)
        famOptions.grpcPort = strdup(options->grpcPort);
    else
        famOptions.grpcPort = strdup("8787");

    optValueMap->insert({supportedOptionList[GRPC_PORT], famOptions.grpcPort});

    if (options && options->libfabricPort)
        famOptions.libfabricPort = strdup(options->libfabricPort);
    else
        famOptions.libfabricPort = strdup("7500");

    optValueMap->insert(
        {supportedOptionList[LIBFABRIC_PORT], famOptions.libfabricPort});

    if (options && options->libfabricProvider)
        famOptions.libfabricProvider = strdup(options->libfabricProvider);
    else
        famOptions.libfabricProvider = strdup("sockets");

    optValueMap->insert({supportedOptionList[LIBFABRIC_PROVIDER],
                         famOptions.libfabricProvider});

    if (options && options->famThreadModel)
        famOptions.famThreadModel = strdup(options->famThreadModel);
    else
        famOptions.famThreadModel = strdup("FAM_THREAD_SERIALIZE");

    if (strcmp(famOptions.famThreadModel, FAM_THREAD_SERIALIZE_STR) == 0)
        famThreadModel = FAM_THREAD_SERIALIZE;
    else if (strcmp(famOptions.famThreadModel, FAM_THREAD_MULTIPLE_STR) == 0)
        famThreadModel = FAM_THREAD_MULTIPLE;
    else {
        message << "Invalid value specified for famThreadModel: "
                << famOptions.famThreadModel;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    optValueMap->insert(
        {supportedOptionList[FAM_THREAD_MODEL], famOptions.famThreadModel});

    if (options && options->allocator)
        famOptions.allocator = strdup(options->allocator);
    else
        famOptions.allocator = strdup("grpc");

    if ((strcmp(famOptions.allocator, FAM_OPTIONS_SHM_STR) != 0) &&
        (strcmp(famOptions.allocator, FAM_OPTIONS_GRPC_STR) != 0)) {
        message << "Invalid value specified for Allocator: "
                << famOptions.famThreadModel;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }

    optValueMap->insert({supportedOptionList[ALLOCATOR], famOptions.allocator});

    if (options && options->famContextModel)
        famOptions.famContextModel = strdup(options->famContextModel);
    else
        famOptions.famContextModel = strdup("FAM_CONTEXT_DEFAULT");

    if (strcmp(famOptions.famContextModel, FAM_CONTEXT_DEFAULT_STR) == 0)
        famContextModel = FAM_CONTEXT_DEFAULT;
    else if (strcmp(famOptions.famContextModel, FAM_CONTEXT_REGION_STR) == 0)
        famContextModel = FAM_CONTEXT_REGION;
    else {
        message << "Invalid value specified for famContextModel: "
                << famOptions.famContextModel;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    optValueMap->insert(
        {supportedOptionList[FAM_CONTEXT_MODEL], famOptions.famContextModel});

    if (options && options->runtime)
        famOptions.runtime = strdup(options->runtime);
    else
        famOptions.runtime = strdup("PMIX");
    if ((strcmp(famOptions.runtime, FAM_OPTIONS_RUNTIME_PMI2_STR) != 0) &&
        (strcmp(famOptions.runtime, FAM_OPTIONS_RUNTIME_NONE_STR) != 0) &&
        (strcmp(famOptions.runtime, FAM_OPTIONS_RUNTIME_PMIX_STR) != 0)) {
        message << "Invalid value specified for Runtime: "
                << famOptions.runtime;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    optValueMap->insert({supportedOptionList[RUNTIME], famOptions.runtime});

    if (options && options->numConsumer)
        famOptions.numConsumer = strdup(options->numConsumer);
    else
        famOptions.numConsumer = strdup("1");
    optValueMap->insert(
        {supportedOptionList[NUM_CONSUMER], famOptions.numConsumer});

    return ret;
}

/**
 * Clean Fam_Options
 */
void fam::Impl_::clean_fam_options() {
    if (optValueMap != NULL) {
        for (auto opt : *optValueMap) {
            free((void *)opt.second);
        }

        optValueMap->clear();
        delete optValueMap;
        optValueMap = NULL;
        memset((void *)&famOptions, 0, sizeof(Fam_Options));
    }
}

/*
 * validate_item - Checks if the descriptot has the a valid key and
 * has the right permissions. Returns 0 on SUCCESS
 * @throws: InvalidOption_Exception if key is invalid/ has insufficient
 * permissions.
 * */
int fam::Impl_::validate_item(Fam_Descriptor *descriptor) {
    std::ostringstream message;
    uint64_t key = descriptor->get_key();

    if (key == FAM_KEY_UNINITIALIZED) {
        famAllocator->check_permission_get_info(descriptor);
    }

    if (key == FAM_KEY_INVALID) {
        message << "Invalid Key Passed" << endl;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    }
    return 0;
}

/**
 * Finalize the fam library. Once finalized, the process can continue work, but
 * it is disconnected from the OpenFAM library functions.
 * @param groupName - name of group of cooperating PEs for this job
 * @see #fam_initialize()
 */
void fam::Impl_::fam_finalize(const char *groupName) {
    FAM_PROFILE_END();

    // Calling destructor for allocator
    if (famAllocator != NULL)
        famAllocator->allocator_finalize();

    // Free up all the options strings
    clean_fam_options();
    // TODO:: Need other closure function, like quiet
    if (famOps != NULL)
        famOps->finalize();
    if (famRuntime != NULL)
        famRuntime->runtime_fini();
}

/**
 * Forcibly terminate all PEs in the same group as the caller
 * @param status - termination status to be returned by the program.
 */
void fam::Impl_::fam_abort(int status) {
    if (famRuntime != NULL)
        famRuntime->runtime_abort(status, "FAM Abort called\n");
    return;
}

/**
 * Suspends the execution of the calling PE until all other PEs issue
 * a call to this particular fam_barrier_all() statement
 */
void fam::Impl_::fam_barrier_all(void) {
    if (famRuntime != NULL)
        famRuntime->runtime_barrier_all();
    return;
}

/**
 * List known options for this version of the library. Provides a way for
 * programs to check which options are known to the library.
 * @return - an array of character strings containing names of options known to
 * the library
 * @see #fam_get_option()
 */
const char **fam::Impl_::fam_list_options(void) {
    size_t buffLen = 0;
    int ndx = 0;
    char **buf, *next;

    while (supportedOptionList[ndx])
        buffLen += strlen(supportedOptionList[ndx++]) + 1;
    buffLen += sizeof(char *) * (ndx + 1);

    buf = (char **)malloc(buffLen);
    memset(buf, 0, buffLen);
    next = (char *)buf + (sizeof(char *) * (ndx + 1));
    ndx = 0;
    while (supportedOptionList[ndx]) {
        size_t optLen = strlen(supportedOptionList[ndx]);
        strncpy(next, supportedOptionList[ndx], optLen);
        next[optLen] = '\0';
        buf[ndx++] = next;
        next = next + optLen + 1;
    }
    return (const char **)buf;
}

/**
 * Query the FAM library for an option.
 * @param optionName - char string containing the name of the option
 * @return pointer to the (read-only) value of the option
 * @throws Fam_InvaidOption_Exception if option not found.
 * @see #fam_list_options()
 */
const void *fam::Impl_::fam_get_option(char *optionName) {
    auto opt = optValueMap->find(optionName);
    if (opt == optValueMap->end()) {
        std::ostringstream message;
        message << "Fam Option not supported: " << optionName;
        throw Fam_InvalidOption_Exception(message.str().c_str());
    } else {
        if (strncmp(optionName, "PE", 2) == 0) {
            int *optVal = (int *)malloc(sizeof(int));
            *optVal = *((int *)opt->second);
            return optVal;
        } else
            return strdup((const char *)opt->second);
    }
}

/**
 * Look up a region in FAM by name in the name service.
 * @param name - name of the region.
 * @return - The descriptor to the region. Null if no such region exists, or if
 * the caller does not have access.
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_lookup
 */
Fam_Region_Descriptor *fam::Impl_::fam_lookup_region(const char *name) {
    FAM_CNTR_INC_API(fam_lookup_region);
    FAM_PROFILE_START_ALLOCATOR(fam_lookup_region);
    uint64_t memoryServerId = generate_memory_server_id(name);
    auto ret = famAllocator->lookup_region(name, memoryServerId);
    FAM_PROFILE_END_ALLOCATOR(fam_lookup_region);
    return ret;
}

/**
 * look up a data item in FAM by name in the name service.
 * @param itemName - name of the data item
 * @param regionName - name of the region containing the data item
 * @return descriptor to the data item if found. Null if no such data item is
 * registered, or if the caller does not have access.
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_lookup_region
 */
Fam_Descriptor *fam::Impl_::fam_lookup(const char *itemName,
                                       const char *regionName) {
    FAM_CNTR_INC_API(fam_lookup);
    FAM_PROFILE_START_ALLOCATOR(fam_lookup);
    uint64_t memoryServerId = generate_memory_server_id(regionName);
    auto ret = famAllocator->lookup(itemName, regionName, memoryServerId);
    FAM_PROFILE_END_ALLOCATOR(fam_lookup);
    return ret;
}

// ALLOCATION Group

/**
 * Allocate a large region of FAM. Regions are primarily used as large
 * containers within which additional memory may be allocated and managed by the
 * program. The API is extensible to support additional (implementation
 * dependent) options. Regions are long-lived and are automatically registered
 * with the name service.
 * @param name - name of the region
 * @param size - size (in bytes) requested for the region. Note that
 * implementations may round up the size to implementation-dependent sizes, and
 * may impose system-wide (or user-dependent) limits on individual and total
 * size allocated to a given user.
 * @param permissions - access permissions to be used for the region
 * @param redundancyLevel - desired redundancy level for the region
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_ALREADYEXIST, FAM_ERR_RPC
 * @return - Region_Descriptor for the created region
 * @see #fam_resize_region
 * @see #fam_destroy_region
 */
Fam_Region_Descriptor *
fam::Impl_::fam_create_region(const char *name, uint64_t size,
                              mode_t permissions,
                              Fam_Redundancy_Level redundancyLevel, ...) {
    FAM_CNTR_INC_API(fam_create_region);
    FAM_PROFILE_START_ALLOCATOR(fam_create_region);
    uint64_t memoryServerId = generate_memory_server_id(name);
    auto ret = famAllocator->create_region(name, size, permissions,
                                           redundancyLevel, memoryServerId);
    FAM_PROFILE_END_ALLOCATOR(fam_create_region);
    return ret;
}

/**
 * Destroy a region, and all contents within the region. Note that this method
 * call will trigger a delayed free operation to permit other instances
 * currently using the region to finish.
 * @param descriptor - descriptor for the region
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_create_region
 * @see #fam_resize_region
 */
void fam::Impl_::fam_destroy_region(Fam_Region_Descriptor *descriptor) {
    FAM_CNTR_INC_API(fam_destroy_region);
    FAM_PROFILE_START_ALLOCATOR(fam_destroy_region);
    famAllocator->destroy_region(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_destroy_region);
    return;
}

/**
 * Resize space allocated to a previously created region.
 * Note that shrinking the size of a region may make descriptors to data items
 * within that region invalid. Thus the method should be used with caution.
 * @param descriptor - descriptor associated with the previously created region
 * @param nbytes - new requested size of the allocated space
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_ALREADYEXIST, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 * @see #fam_create_region
 * @see #fam_destroy_region
 */
int fam::Impl_::fam_resize_region(Fam_Region_Descriptor *descriptor,
                                  uint64_t nbytes) {
    FAM_CNTR_INC_API(fam_resize_region);
    FAM_PROFILE_START_ALLOCATOR(fam_resize_region);
    famAllocator->resize_region(descriptor, nbytes);
    FAM_PROFILE_END_ALLOCATOR(fam_resize_region);
    return 0;
}

/**
 * Allocate some unnamed space within a region. Allocates an area of FAM within
 * a region
 * @param name - (optional) name of the data item
 * @param nybtes - size of the space to allocate in bytes.
 * @param accessPermissions - permissions associated with this space
 * @param region - descriptor of the region within which the space is being
 * allocated. If not present or null, a default region is used.
 * @return - descriptor that can be used within the program to refer to this
 * space
 * @see #fam_deallocate()
 */
Fam_Descriptor *fam::Impl_::fam_allocate(uint64_t nbytes,
                                         mode_t accessPermissions,
                                         Fam_Region_Descriptor *region) {
    FAM_CNTR_INC_API(fam_allocate);
    FAM_PROFILE_START_ALLOCATOR(fam_allocate);
    auto ret = fam_allocate("", nbytes, accessPermissions, region);
    FAM_PROFILE_END_ALLOCATOR(fam_allocate);
    return ret;
}
Fam_Descriptor *fam::Impl_::fam_allocate(const char *name, uint64_t nbytes,
                                         mode_t accessPermissions,
                                         Fam_Region_Descriptor *region) {
    FAM_CNTR_INC_API(fam_allocate);
    FAM_PROFILE_START_ALLOCATOR(fam_allocate);
    auto ret = famAllocator->allocate(name, nbytes, accessPermissions, region);
    FAM_PROFILE_END_ALLOCATOR(fam_allocate);
    return ret;
}

/**
 * Deallocate allocated space in memory
 * @param descriptor - descriptor associated with the space.
 * @see #fam_allocate()
 */
void fam::Impl_::fam_deallocate(Fam_Descriptor *descriptor) {
    FAM_CNTR_INC_API(fam_deallocate);
    FAM_PROFILE_START_ALLOCATOR(fam_deallocate);
    famAllocator->deallocate(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_deallocate);
    return;
}

/**
 * Change permissions associated with a data item descriptor.
 * @param descriptor - descriptor associated with some data item
 * @param accessPermissions - new permissions for the data item
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 */
int fam::Impl_::fam_change_permissions(Fam_Descriptor *descriptor,
                                       mode_t accessPermissions) {
    FAM_CNTR_INC_API(fam_change_permissions);
    FAM_PROFILE_START_ALLOCATOR(fam_change_permissions);
    famAllocator->change_permission(descriptor, accessPermissions);
    FAM_PROFILE_END_ALLOCATOR(fam_change_permissions);
    return 0;
}

/**
 * Change permissions associated with a region descriptor.
 * @param descriptor - descriptor associated with some region
 * @param accessPermissions - new permissions for the region
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 */
int fam::Impl_::fam_change_permissions(Fam_Region_Descriptor *descriptor,
                                       mode_t accessPermissions) {
    FAM_CNTR_INC_API(fam_change_permissions);
    FAM_PROFILE_START_ALLOCATOR(fam_change_permissions);
    famAllocator->change_permission(descriptor, accessPermissions);
    FAM_PROFILE_END_ALLOCATOR(fam_change_permissions);
    return 0;
}

/**
 * Get the size, permissions and name of the data item associated with
 * a data item descriptor.
 * @param descriptor - descriptor associated with some data item.
 * @param famInfo - Structure that holds the size, permission and
 * name info for the specified data item descriptor.
 * @return - none
 * @throws Fam_Exception - if invalid Fam_Descriptor is passed.
 */
void fam::Impl_::fam_stat(Fam_Descriptor *descriptor, Fam_Stat *famInfo) {

    Fam_Region_Item_Info itemInfo;

    if ((descriptor->get_desc_status() == DESC_INIT_DONE) ||
        (descriptor->get_desc_status() == DESC_INIT_DONE_BUT_KEY_NOT_VALID)) {
        famInfo->size = descriptor->get_size();
        famInfo->perm = descriptor->get_perm();
        famInfo->name = descriptor->get_name();

    } else if (descriptor->get_desc_status() == DESC_INVALID) {
        std::ostringstream message;
        message << "Descriptor is no longer valid" << endl;
        throw Fam_Exception(message.str().c_str());
    } else {
        itemInfo = famAllocator->get_stat_info(descriptor);
        famInfo->size = itemInfo.size;
        famInfo->perm = itemInfo.perm;
        famInfo->name = itemInfo.name;
    }
}

/**
 * Get the size, permissions and name of the region associated
 * with a region descriptor.
 * @param descriptor - descriptor associated with some region.
 * @param famInfo - Structure that holds the size, permission and
 * name infor for the specified region descriptor.
 * @return - none
 * @throws Fam_Exception - if invalid Fam_Region_Descriptor is passed.
 */
void fam::Impl_::fam_stat(Fam_Region_Descriptor *descriptor,
                          Fam_Stat *famInfo) {
    Fam_Region_Item_Info regionInfo;

    if ((descriptor->get_desc_status() == DESC_INIT_DONE) ||
        (descriptor->get_desc_status() == DESC_INIT_DONE_BUT_KEY_NOT_VALID)) {
        famInfo->size = descriptor->get_size();
        famInfo->perm = descriptor->get_perm();
        famInfo->name = descriptor->get_name();
    } else if (descriptor->get_desc_status() == DESC_INVALID) {
        std::ostringstream message;
        message << "Descriptor is no longer valid" << endl;
        throw Fam_Exception(message.str().c_str());
    } else {
        regionInfo = famAllocator->check_permission_get_info(descriptor);
        famInfo->size = regionInfo.size;
        famInfo->perm = regionInfo.perm;
        famInfo->name = regionInfo.name;
    }
}

/**
 * Map a data item in FAM to the local virtual address space, and return its
 * pointer.
 * @param descriptor - descriptor to be mapped
 * @return pointer within the process virtual address space that can be used to
 * directly access the data item in FAM
 * @see #fam_unmap()
 */
void *fam::Impl_::fam_map(Fam_Descriptor *descriptor) {
    void *result = NULL;
    FAM_CNTR_INC_API(fam_map);
    FAM_PROFILE_START_ALLOCATOR(fam_map);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }
    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_map);

    FAM_PROFILE_START_OPS(fam_map);
    if (ret == 0) {
        void *address;
        address = famAllocator->fam_map(descriptor);
        if (address != NULL) {
            descriptor->set_base_address(address);
        }
        result = address;
    }
    FAM_PROFILE_END_OPS(fam_map);
    return result;
}

/**
 * Unmap a data item in FAM from the local virtual address space.
 * @param local - pointer within the process virtual address space to be
 * unmapped
 * @param descriptor - descriptor for the FAM to be unmapped
 * @see #fam_map()
 */
void fam::Impl_::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    FAM_CNTR_INC_API(fam_unmap);
    FAM_PROFILE_START_ALLOCATOR(fam_unmap);
    if (descriptor == NULL || local == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_unmap);
    FAM_PROFILE_START_OPS(fam_unmap);
    if (ret == 0) {
        famAllocator->fam_unmap(local, descriptor);
    }
    FAM_PROFILE_END_OPS(fam_unmap);
    return;
}

// DATA READ AND WRITE Group. These APIs read and write data in FAM and copy
// data between local DRAM and FAM.

// DATA GET/PUT sub-group

/**
 * Copy data from FAM to node local memory, blocking the caller while the copy
 * is completed.
 * @param descriptor - valid descriptor to area in FAM.
 * @param local - pointer to local memory region where data needs to be copied.
 * Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor from
 * where memory should be copied
 * @param nbytes - number of bytes to be copied from global to local memory
 * @return - 0 for successful completion, 1 for unsuccessful, and a negative
 * number in case of exceptions.
 * @throws : Fam_InvalidOption_Exception if incorrect parameters are passed.
 * @throws : Fam_Permission_Exception if the given key has incorrect permissions
 * @throws : Fam_Datapath_Exception if libfabric read fails
 */
int fam::Impl_::fam_get_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t offset, uint64_t nbytes) {

    int ret = 0;
    std::ostringstream message;

    FAM_CNTR_INC_API(fam_get_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_get_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nbytes == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }
    ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_get_blocking);
    FAM_PROFILE_START_OPS(fam_get_blocking);
    if (ret == 0) {
        // Read data from FAM region with this key
        ret = famOps->get_blocking(local, descriptor, offset, nbytes);
    }
    FAM_PROFILE_END_OPS(fam_get_blocking);
    return ret;
}

/**
 * Initiate a copy of data from FAM to node local memory. Do not wait until copy
 * is finished
 * @param descriptor - valid descriptor to area in FAM.
 * @param local - pointer to local memory region where data needs to be copied.
 * Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor from
 * where memory should be copied
 * @param nbytes - number of bytes to be copied from global to local memory
 */
void fam::Impl_::fam_get_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t offset, uint64_t nbytes) {

    FAM_CNTR_INC_API(fam_get_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_get_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nbytes == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }
    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_get_nonblocking);
    FAM_PROFILE_START_OPS(fam_get_nonblocking);
    if (ret == 0) {
        // Read data from FAM region with this key
        famOps->get_nonblocking(local, descriptor, offset, nbytes);
    }
    FAM_PROFILE_END_OPS(fam_get_nonblocking);
    return;
}

/**
 * Copy data from local memory to FAM, blocking until the copy is complete.
 * @param local - pointer to local memory. Must point to valid data in local
 * memory
 * @param descriptor - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor to
 * where data should be copied
 * @param nbytes - number of bytes to be copied from local to FAM
 * @return - 0 for successful completion, 1 for unsuccessful completion,
 * negative number in case of exceptions
 * @throws : Fam_InvalidOption_Exception if incorrect parameters are passed.
 * @throws : Fam_Permission_Exception if the given key has incorrect permissions
 * @throws : Fam_Datapath_Exception if libfabric write fails.
 */
int fam::Impl_::fam_put_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t offset, uint64_t nbytes) {

    int ret = 0;
    std::ostringstream message;

    FAM_CNTR_INC_API(fam_put_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_put_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nbytes == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
        return false;
    }

    ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_put_blocking);
    FAM_PROFILE_START_OPS(fam_put_blocking);
    if (ret == 0) {
        ret = famOps->put_blocking(local, descriptor, offset, nbytes);
    }
    FAM_PROFILE_END_OPS(fam_put_blocking);
    return ret;
}

/**
 * Initiate a copy of data from local memory to FAM, returning before copy is
 * complete
 * @param local - pointer to local memory. Must point to valid data in local
 * memory
 * @param descriptor - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor to
 * where data should be copied
 * @param nbytes - number of bytes to be copied from local to FAM
 */
void fam::Impl_::fam_put_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t offset, uint64_t nbytes) {
    FAM_CNTR_INC_API(fam_put_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_put_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nbytes == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_put_nonblocking);
    FAM_PROFILE_START_OPS(fam_put_nonblocking);
    if (ret == 0) {
        famOps->put_nonblocking(local, descriptor, offset, nbytes);
    }
    FAM_PROFILE_END_OPS(fam_put_nonblocking);
    return;
}

// LOAD/STORE sub-group

// GATHER/SCATTER subgroup

/**
 * Gather data from FAM to local memory, blocking while the copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param firstElement - first element in FAM to include in the strided access
 * @param stride - stride in elements
 * @param elementSize - size of the element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case of exception
 * @see #fam_scatter_strided
 */
int fam::Impl_::fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t nElements, uint64_t firstElement,
                                    uint64_t stride, uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_gather_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_gather_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_gather_blocking);
    FAM_PROFILE_START_OPS(fam_gather_blocking);
    if (ret == 0) {
        ret = famOps->gather_blocking(local, descriptor, nElements,
                                      firstElement, stride, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_gather_blocking);
    return ret;
}

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param elementIndex - array of element indexes in FAM to fetch
 * @param elementSize - size of each element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_scatter_indexed
 */
int fam::Impl_::fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t nElements, uint64_t *elementIndex,
                                    uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_gather_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_gather_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_gather_blocking);
    FAM_PROFILE_START_OPS(fam_gather_blocking);
    if (ret == 0) {
        ret = famOps->gather_blocking(local, descriptor, nElements,
                                      elementIndex, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_gather_blocking);
    return ret;
}

/**
 * Initiate a gather of data from FAM to local memory, return before completion
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param firstElement - first element in FAM to include in the strided access
 * @param stride - stride in elements
 * @param elementSize - size of the element in bytes
 * @see #fam_scatter_strided
 */
void fam::Impl_::fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t firstElement, uint64_t stride,
                                        uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_gather_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_gather_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_gather_nonblocking);
    FAM_PROFILE_START_OPS(fam_gather_nonblocking);
    if (ret == 0) {
        famOps->gather_nonblocking(local, descriptor, nElements, firstElement,
                                   stride, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_gather_nonblocking);
    return;
}

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param elementIndex - array of element indexes in FAM to fetch
 * @param elementSize - size of each element in bytes
 * @see #fam_scatter_indexed
 */
void fam::Impl_::fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                        uint64_t nElements,
                                        uint64_t *elementIndex,
                                        uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_gather_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_gather_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_gather_nonblocking);
    FAM_PROFILE_START_OPS(fam_gather_nonblocking);
    if (ret == 0) {
        famOps->gather_nonblocking(local, descriptor, nElements, elementIndex,
                                   elementSize);
    }
    FAM_PROFILE_END_OPS(fam_gather_nonblocking);
    return;
}

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param firstElement - placement of the first element in FAM to place for the
 * strided access
 * @param stride - stride in elements
 * @param elementSize - size of each element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_strided
 */
int fam::Impl_::fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t firstElement,
                                     uint64_t stride, uint64_t elementSize) {

    FAM_CNTR_INC_API(fam_scatter_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_scatter_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_scatter_blocking);
    FAM_PROFILE_START_OPS(fam_scatter_blocking);
    if (ret == 0) {
        ret = famOps->scatter_blocking(local, descriptor, nElements,
                                       firstElement, stride, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_scatter_blocking);
    return ret;
}

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing data elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param elementIndex - array containing element indexes
 * @param elementSize - size of the element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_indexed
 */
int fam::Impl_::fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t *elementIndex,
                                     uint64_t elementSize) {

    FAM_CNTR_INC_API(fam_scatter_blocking);
    FAM_PROFILE_START_ALLOCATOR(fam_scatter_blocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_scatter_blocking);
    FAM_PROFILE_START_OPS(fam_scatter_blocking);
    if (ret == 0) {
        ret = famOps->scatter_blocking(local, descriptor, nElements,
                                       elementIndex, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_scatter_blocking);
    return ret;
}

/**
 * initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param firstElement - placement of the first element in FAM to place for the
 * strided access
 * @param stride - stride in elements
 * @param elementSize - size of each element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_strided
 */
void fam::Impl_::fam_scatter_nonblocking(void *local,
                                         Fam_Descriptor *descriptor,
                                         uint64_t nElements,
                                         uint64_t firstElement, uint64_t stride,
                                         uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_scatter_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_scatter_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_scatter_nonblocking);
    FAM_PROFILE_START_OPS(fam_scatter_nonblocking);
    if (ret == 0) {
        famOps->scatter_nonblocking(local, descriptor, nElements, firstElement,
                                    stride, elementSize);
    }
    FAM_PROFILE_END_OPS(fam_scatter_nonblocking);
    return;
}

/**
 * Initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing data elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param elementIndex - array containing element indexes
 * @param elementSize - size of the element in bytes
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_indexed
 */
void fam::Impl_::fam_scatter_nonblocking(void *local,
                                         Fam_Descriptor *descriptor,
                                         uint64_t nElements,
                                         uint64_t *elementIndex,
                                         uint64_t elementSize) {
    FAM_CNTR_INC_API(fam_scatter_nonblocking);
    FAM_PROFILE_START_ALLOCATOR(fam_scatter_nonblocking);
    if ((local == NULL) || (descriptor == NULL) || (nElements == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_scatter_nonblocking);
    FAM_PROFILE_START_OPS(fam_scatter_nonblocking);
    if (ret == 0) {
        famOps->scatter_nonblocking(local, descriptor, nElements, elementIndex,
                                    elementSize);
    }
    FAM_PROFILE_END_OPS(fam_scatter_nonblocking);
    return;
}

// COPY Subgroup

/**
 * Copy data from one FAM-resident data item to another FAM-resident data item
 * (potentially within a different region).
 * @param src - valid descriptor to source data item in FAM.
 * @param srcOffset - byte offset within the space defined by the src descriptor
 * from which memory should be copied.
 * @param dest - valid descriptor to destination data item in FAM.
 * @param destOffset - byte offset within the space defined by the dest
 * descriptor to which memory should be copied.
 * @param nbytes - number of bytes to be copied
 *
 * Note : In case of copy operation across memoryserver this API is blocking
 * and no need to wait on copy.
 */
void *fam::Impl_::fam_copy(Fam_Descriptor *src, uint64_t srcOffset,
                           Fam_Descriptor *dest, uint64_t destOffset,
                           uint64_t nbytes) {
    void *result = NULL;
    FAM_CNTR_INC_API(fam_copy);
    FAM_PROFILE_START_ALLOCATOR(fam_copy);
    if ((src == NULL) || (nbytes == 0)) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int retS = validate_item(src);
    int retD = validate_item(dest);
    FAM_PROFILE_END_ALLOCATOR(fam_copy);
    FAM_PROFILE_START_OPS(fam_copy);
    if (retS == 0 && retD == 0) {
        result = famOps->copy(src, srcOffset, dest, destOffset, nbytes);
    }
    FAM_PROFILE_END_OPS(fam_copy);
    return result;
}

void fam::Impl_::fam_copy_wait(void *waitObj) {
    FAM_CNTR_INC_API(fam_copy_wait);
    FAM_PROFILE_START_ALLOCATOR(fam_copy_wait);
    if (waitObj == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    famOps->wait_for_copy(waitObj);
    FAM_PROFILE_END_ALLOCATOR(fam_copy_wait);
    return;
}

// ATOMICS Group

// NON fetching routines

/**
 * set group - atomically set a value at a given offset within a data item in
 * FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be set at the given location
 */
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);
    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         int128_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}

void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}
void fam::Impl_::fam_set(Fam_Descriptor *descriptor, uint64_t offset,
                         double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_set);
    FAM_PROFILE_START_ALLOCATOR(fam_set);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_set);

    FAM_PROFILE_START_OPS(fam_set);
    if (ret == 0) {
        famOps->atomic_set(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_set);
    return;
}

/**
 * add group - atomically add a value to the value at a given offset within a
 * data item in FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be added to the existing value at the given location
 */
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}
void fam::Impl_::fam_add(Fam_Descriptor *descriptor, uint64_t offset,
                         double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_add);
    FAM_PROFILE_START_ALLOCATOR(fam_add);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_add);

    FAM_PROFILE_START_OPS(fam_add);
    if (ret == 0) {
        famOps->atomic_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_add);
    return;
}

/**
 * subtract group - atomically subtract a value from a value at a given offset
 * within a data item in FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be subtracted from the existing value at the given
 * location
 */
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}
void fam::Impl_::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_subtract);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_subtract);

    FAM_PROFILE_START_OPS(fam_subtract);
    if (ret == 0) {
        famOps->atomic_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_subtract);
    return;
}

/**
 * min group - atomically set the value at a given offset within a data item in
 * FAM to the smaller of the existing value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared to the existing value at the given
 * location
 */
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}
void fam::Impl_::fam_min(Fam_Descriptor *descriptor, uint64_t offset,
                         double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_min);
    FAM_PROFILE_START_ALLOCATOR(fam_min);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_min);

    FAM_PROFILE_START_OPS(fam_min);
    if (ret == 0) {
        famOps->atomic_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_min);
    return;
}

/**
 * max group - atomically set the value at a given offset within a data item in
 * FAM to the larger of the value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared to the existing value at the given
 * location
 */
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}
void fam::Impl_::fam_max(Fam_Descriptor *descriptor, uint64_t offset,
                         double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_max);
    FAM_PROFILE_START_ALLOCATOR(fam_max);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_max);

    FAM_PROFILE_START_OPS(fam_max);
    if (ret == 0) {
        famOps->atomic_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_max);
    return;
}

/**
 * and group - atomically replace the value at a given offset within a data item
 * in FAM with the logical AND of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 */
void fam::Impl_::fam_and(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_and);
    FAM_PROFILE_START_ALLOCATOR(fam_and);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_and);

    FAM_PROFILE_START_OPS(fam_and);
    if (ret == 0) {
        famOps->atomic_and(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_and);
    return;
}
void fam::Impl_::fam_and(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_and);
    FAM_PROFILE_START_ALLOCATOR(fam_and);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_and);

    FAM_PROFILE_START_OPS(fam_and);
    if (ret == 0) {
        famOps->atomic_and(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_and);
    return;
}

/**
 * or group - atomically replace the value at a given offset within a data item
 * in FAM with the logical OR of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 */
void fam::Impl_::fam_or(Fam_Descriptor *descriptor, uint64_t offset,
                        uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_or);
    FAM_PROFILE_START_ALLOCATOR(fam_or);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_or);

    FAM_PROFILE_START_OPS(fam_or);
    if (ret == 0) {
        famOps->atomic_or(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_or);
    return;
}
void fam::Impl_::fam_or(Fam_Descriptor *descriptor, uint64_t offset,
                        uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_or);
    FAM_PROFILE_START_ALLOCATOR(fam_or);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_or);

    FAM_PROFILE_START_OPS(fam_or);
    if (ret == 0) {
        famOps->atomic_or(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_or);
    return;
}

/**
 * xor group - atomically replace the value at a given offset within a data item
 * in FAM with the logical XOR of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 */
void fam::Impl_::fam_xor(Fam_Descriptor *descriptor, uint64_t offset,
                         uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_xor);
    FAM_PROFILE_START_ALLOCATOR(fam_xor);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_xor);

    FAM_PROFILE_START_OPS(fam_xor);
    if (ret == 0) {
        famOps->atomic_xor(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_xor);
    return;
}
void fam::Impl_::fam_xor(Fam_Descriptor *descriptor, uint64_t offset,
                         uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_xor);
    FAM_PROFILE_START_ALLOCATOR(fam_xor);
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_xor);

    FAM_PROFILE_START_OPS(fam_xor);
    if (ret == 0) {
        famOps->atomic_xor(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_xor);
    return;
}

// FETCHING Routines - perform the operation, and return the old value in FAM

/**
 * fetch group - atomically fetches the value at the given offset within a data
 * item from FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @return - value from the given location in FAM
 */
int32_t fam::Impl_::fam_fetch_int32(Fam_Descriptor *descriptor,
                                    uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    int32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_int32(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}
int64_t fam::Impl_::fam_fetch_int64(Fam_Descriptor *descriptor,
                                    uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    int64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_int64(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}
int128_t fam::Impl_::fam_fetch_int128(Fam_Descriptor *descriptor,
                                      uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    int128_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_int128(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}

uint32_t fam::Impl_::fam_fetch_uint32(Fam_Descriptor *descriptor,
                                      uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    uint32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_uint32(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}
uint64_t fam::Impl_::fam_fetch_uint64(Fam_Descriptor *descriptor,
                                      uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    uint64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_uint64(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}
float fam::Impl_::fam_fetch_float(Fam_Descriptor *descriptor, uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    float res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_float(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}
double fam::Impl_::fam_fetch_double(Fam_Descriptor *descriptor,
                                    uint64_t offset) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch);
    double res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch);

    FAM_PROFILE_START_OPS(fam_fetch);
    if (ret == 0) {
        res = famOps->atomic_fetch_double(descriptor, offset);
    }
    FAM_PROFILE_END_OPS(fam_fetch);
    return res;
}

/**
 * swap group - atomically replace the value at the given offset within a data
 * item in FAM with the given value, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be swapped with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    int32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}
int64_t fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    int64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}
uint32_t fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    uint32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}
uint64_t fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    uint64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}
float fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                           float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    float res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}
double fam::Impl_::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                            double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_swap);
    double res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_swap);

    FAM_PROFILE_START_OPS(fam_swap);
    if (ret == 0) {
        res = famOps->swap(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_swap);
    return res;
}

/**
 * compare and swap group - atomically conditionally replace the value at the
 * given offset within a data item in FAM with the given value, and return the
 * old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param oldValue - value to be compared with the existing value at the given
 * location
 * @param newValue - new value to be stored if comparison is successful
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_compare_swap(Fam_Descriptor *descriptor,
                                     uint64_t offset, int32_t oldValue,
                                     int32_t newValue) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_compare_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_compare_swap);
    int32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_compare_swap);

    FAM_PROFILE_START_OPS(fam_compare_swap);
    if (ret == 0) {
        res = famOps->compare_swap(descriptor, offset, oldValue, newValue);
    }
    FAM_PROFILE_END_OPS(fam_compare_swap);
    return res;
}
int64_t fam::Impl_::fam_compare_swap(Fam_Descriptor *descriptor,
                                     uint64_t offset, int64_t oldValue,
                                     int64_t newValue) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_compare_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_compare_swap);
    int64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_compare_swap);

    FAM_PROFILE_START_OPS(fam_compare_swap);
    if (ret == 0) {
        res = famOps->compare_swap(descriptor, offset, oldValue, newValue);
    }
    FAM_PROFILE_END_OPS(fam_compare_swap);
    return res;
}
uint32_t fam::Impl_::fam_compare_swap(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t oldValue,
                                      uint32_t newValue) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_compare_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_compare_swap);
    uint32_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_compare_swap);

    FAM_PROFILE_START_OPS(fam_compare_swap);
    if (ret == 0) {
        res = famOps->compare_swap(descriptor, offset, oldValue, newValue);
    }
    FAM_PROFILE_END_OPS(fam_compare_swap);
    return res;
}
uint64_t fam::Impl_::fam_compare_swap(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t oldValue,
                                      uint64_t newValue) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_compare_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_compare_swap);
    uint64_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_compare_swap);

    FAM_PROFILE_START_OPS(fam_compare_swap);
    if (ret == 0) {
        res = famOps->compare_swap(descriptor, offset, oldValue, newValue);
    }
    FAM_PROFILE_END_OPS(fam_compare_swap);
    return res;
}
int128_t fam::Impl_::fam_compare_swap(Fam_Descriptor *descriptor,
                                      uint64_t offset, int128_t oldValue,
                                      int128_t newValue) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_compare_swap);
    FAM_PROFILE_START_ALLOCATOR(fam_compare_swap);
    int128_t res = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_compare_swap);

    FAM_PROFILE_START_OPS(fam_compare_swap);
    if (ret == 0) {
        res = famOps->compare_swap(descriptor, offset, oldValue, newValue);
    }
    FAM_PROFILE_END_OPS(fam_compare_swap);
    return res;
}

/**
 * fetch and add group - atomically add the given value to the value at the
 * given offset within a data item in FAM, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be added to the existing value at the given location
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    int32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}
int64_t fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    int64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}
uint32_t fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}

uint64_t fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}

float fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    float old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}
double fam::Impl_::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                 double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_add);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_add);
    double old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_add);

    FAM_PROFILE_START_OPS(fam_fetch_add);
    if (ret == 0) {
        old = famOps->atomic_fetch_add(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_add);
    return old;
}

/**
 * fetch and subtract group - atomically subtract the given value from the value
 * at the given offset within a data item in FAM, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be subtracted from the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                       uint64_t offset, int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    int32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}
int64_t fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                       uint64_t offset, int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    int64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}
uint32_t fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, uint32_t value) {

    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}
uint64_t fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, uint64_t value) {

    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}
float fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                     uint64_t offset, float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    float old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}
double fam::Impl_::fam_fetch_subtract(Fam_Descriptor *descriptor,
                                      uint64_t offset, double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_subtract);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_subtract);
    double old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_subtract);

    FAM_PROFILE_START_OPS(fam_fetch_subtract);
    if (ret == 0) {
        old = famOps->atomic_fetch_subtract(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_subtract);
    return old;
}

/**
 * fetch and min group - atomically set the value at a given offset within a
 * data item in FAM to the smaller of the existing value and the given value,
 * and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    int32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}
int64_t fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    int64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}
uint32_t fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}
uint64_t fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}
float fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    float old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}
double fam::Impl_::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                 double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_min);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_min);
    double old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_min);

    FAM_PROFILE_START_OPS(fam_fetch_min);
    if (ret == 0) {
        old = famOps->atomic_fetch_min(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_min);
    return old;
}

/**
 * fetch and max group - atomically set the value at a given offset within a
 * data item in FAM to the larger of the existing value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
int32_t fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                  int32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    int32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}
int64_t fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                  int64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    int64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}
uint32_t fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}
uint64_t fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}
float fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                float value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    float old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}
double fam::Impl_::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                 double value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_max);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_max);
    double old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_max);

    FAM_PROFILE_START_OPS(fam_fetch_max);
    if (ret == 0) {
        old = famOps->atomic_fetch_max(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_max);
    return old;
}

/**
 * fetch and and group - atomically replace the value at a given offset within a
 * data item in FAM with the logical AND of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
uint32_t fam::Impl_::fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_and);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_and);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_and);

    FAM_PROFILE_START_OPS(fam_fetch_and);
    if (ret == 0) {
        old = famOps->atomic_fetch_and(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_and);
    return old;
}
uint64_t fam::Impl_::fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_and);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_and);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_and);

    FAM_PROFILE_START_OPS(fam_fetch_and);
    if (ret == 0) {
        old = famOps->atomic_fetch_and(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_and);
    return old;
}

/**
 * fetch and or group - atomically replace the value at a given offset within a
 * data item in FAM with the logical OR of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
uint32_t fam::Impl_::fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_or);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_or);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_or);

    FAM_PROFILE_START_OPS(fam_fetch_or);
    if (ret == 0) {
        old = famOps->atomic_fetch_or(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_or);
    return old;
}
uint64_t fam::Impl_::fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_or);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_or);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_or);

    FAM_PROFILE_START_OPS(fam_fetch_or);
    if (ret == 0) {
        old = famOps->atomic_fetch_or(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_or);
    return old;
}

/**
 * fetch and xor group - atomically replace the value at a given offset within a
 * data item in FAM with the logical XOR of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - old value from the given location in FAM
 */
uint32_t fam::Impl_::fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint32_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_xor);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_xor);
    uint32_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_xor);

    FAM_PROFILE_START_OPS(fam_fetch_xor);
    if (ret == 0) {
        old = famOps->atomic_fetch_xor(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_xor);
    return old;
}
uint64_t fam::Impl_::fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                                   uint64_t value) {
    std::ostringstream message;
    FAM_CNTR_INC_API(fam_fetch_xor);
    FAM_PROFILE_START_ALLOCATOR(fam_fetch_xor);
    uint64_t old = 0;
    if (descriptor == NULL) {
        throw Fam_InvalidOption_Exception("Invalid Options");
    }

    int ret = validate_item(descriptor);
    FAM_PROFILE_END_ALLOCATOR(fam_fetch_xor);

    FAM_PROFILE_START_OPS(fam_fetch_xor);
    if (ret == 0) {
        old = famOps->atomic_fetch_xor(descriptor, offset, value);
    }
    FAM_PROFILE_END_OPS(fam_fetch_xor);
    return old;
}

// MEMORY ORDERING Routines - provide ordering of FAM operations issued by a PE

/**
 * fam_fence - ensures that FAM operations (put, scatter, atomics, copy) issued
 * by the calling PE thread before the fence are ordered before FAM operations
 * issued by the calling thread after the fence Note that method this does NOT
 * order load/store accesses by the processor to FAM enabled by fam_map().
 */
void fam::Impl_::fam_fence(Fam_Region_Descriptor *descriptor) {
    FAM_CNTR_INC_API(fam_fence);
    FAM_PROFILE_START_OPS(fam_fence);
    famOps->fence(descriptor);
    FAM_PROFILE_END_OPS(fam_fence);
    return;
}

/**
 * fam_quiet - blocks the calling PE thread until all its pending FAM operations
 * (put, scatter, atomics, copy) are completed. Note that method this does NOT
 * order or wait for completion of load/store accesses by the processor to FAM
 * enabled by fam_map().
 */
void fam::Impl_::fam_quiet(Fam_Region_Descriptor *descriptor) {
    FAM_CNTR_INC_API(fam_quiet);
    FAM_PROFILE_START_OPS(fam_quiet);
    famOps->quiet(descriptor);
    FAM_PROFILE_END_OPS(fam_quiet);
    return;
}

/**
 * Initialize the OpenFAM library. This method is required to be the first
 * method called when a process uses the OpenFAM library.
 * @param groupName - name of the group of cooperating PEs.
 * @param options - options structure containing initialization choices
 * @throws Fam_InvalidOption_Exception - for invalid option values
 * @throws Fam_Datapath_Exception - for fabric initialization failure
 * @throws Fam_Allocator_Exception - for allocator grpc initialization failure
 * @throws Fam_Pmi_Exception - for runtime initialization failure
 * @return - {true(0), false(1), errNo(<0)}
 * @see #fam_finalize()
 * @see #Fam_Options
 */
int fam::fam_initialize(const char *groupName, Fam_Options *options) {
    return pimpl_->fam_initialize(groupName, options);
}

/**
 * Finalize the fam library. Once finalized, the process can continue work, but
 * it is disconnected from the OpenFAM library functions.
 * @param groupName - name of group of cooperating PEs for this job
 * @see #fam_initialize()
 */
void fam::fam_finalize(const char *groupName) {
    pimpl_->fam_finalize(groupName);
}

/**
 * Forcibly terminate all PEs in the same group as the caller
 * @param status - termination status to be returned by the program.
 */
void fam::fam_abort(int status) { pimpl_->fam_abort(status); }

/**
 * Suspends the execution of the calling PE until all other PEs issue
 * a call to this particular fam_barrier_all() statement
 */
void fam::fam_barrier_all(void) { pimpl_->fam_barrier_all(); }

/**
 * List known options for this version of the library. Provides a way for
 * programs to check which options are known to the library.
 * @return - an array of character strings containing names of options known to
 * the library
 * @see #fam_get_option()
 */
const char **fam::fam_list_options(void) { return pimpl_->fam_list_options(); }

/**
 * Query the FAM library for an option.
 * @param optionName - char string containing the name of the option
 * @throws Fam_InvaidOption_Exception if option not found.
 * @return pointer to the (read-only) value of the option
 * @see #fam_list_options()
 */
const void *fam::fam_get_option(char *optionName) {
    return pimpl_->fam_get_option(optionName);
}

/**
 * Look up a region in FAM by name in the name service.
 * @param name - name of the region.
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - The descriptor to the region. Null if no such region exists, or if
 * the caller does not have access.
 * @see #fam_lookup
 */
Fam_Region_Descriptor *fam::fam_lookup_region(const char *name) {
    return pimpl_->fam_lookup_region(name);
}

/**
 * look up a data item in FAM by name in the name service.
 * @param itemName - name of the data item
 * @param regionName - name of the region containing the data item
 * @throws Fam_Allocator_Exception - excptObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return descriptor to the data item if found. Null if no such data item is
 * registered, or if the caller does not have access.
 * @see #fam_lookup_region
 */
Fam_Descriptor *fam::fam_lookup(const char *itemName, const char *regionName) {
    return pimpl_->fam_lookup(itemName, regionName);
}

// ALLOCATION Group

/**
 * Allocate a large region of FAM. Regions are primarily used as large
 * containers within which additional memory may be allocated and managed by the
 * program. The API is extensible to support additional (implementation
 * dependent) options. Regions are long-lived and are automatically registered
 * with the name service.
 * @param name - name of the region
 * @param size - size (in bytes) requested for the region. Note that
 * implementations may round up the size to implementation-dependent sizes, and
 * may impose system-wide (or user-dependent) limits on individual and total
 * size allocated to a given user.
 * @param permissions - access permissions to be used for the region
 * @param redundancyLevel - desired redundancy level for the region
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_ALREADYEXIST, FAM_ERR_RPC
 * @return - Region_Descriptor for the created region
 * @see #fam_resize_region
 * @see #fam_destroy_region
 */
Fam_Region_Descriptor *
fam::fam_create_region(const char *name, uint64_t size, mode_t permissions,
                       Fam_Redundancy_Level redundancyLevel, ...) {
    return pimpl_->fam_create_region(name, size, permissions, redundancyLevel);
}

/**
 * Destroy a region, and all contents within the region. Note that this method
 * call will trigger a delayed free operation to permit other instances
 * currently using the region to finish.
 * @param descriptor - descriptor for the region
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_create_region
 * @see #fam_resize_region
 */
void fam::fam_destroy_region(Fam_Region_Descriptor *descriptor) {
    pimpl_->fam_destroy_region(descriptor);
}

/**
 * Resize space allocated to a previously created region.
 * Note that shrinking the size of a region may make descriptors to data items
 * within that region invalid. Thus the method should be used with caution.
 * @param descriptor - descriptor associated with the previously created region
 * @param nbytes - new requested size of the allocated space
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 * @see #fam_create_region
 * @see #fam_destroy_region
 */
int fam::fam_resize_region(Fam_Region_Descriptor *descriptor, uint64_t nbytes) {
    return pimpl_->fam_resize_region(descriptor, nbytes);
}

/**
 * Allocate some unnamed space within a region. Allocates an area of FAM within
 * a region
 * @param name - (optional) name of the data item
 * @param nybtes - size of the space to allocate in bytes.
 * @param accessPermissions - permissions associated with this space
 * @param region - descriptor of the region within which the space is being
 * allocated. If not present or null, a default region is used.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_ALREADYEXIST, FAM_ERR_RPC
 * @return - descriptor that can be used within the program to refer to this
 * space
 * @see #fam_deallocate()
 */
Fam_Descriptor *fam::fam_allocate(uint64_t nbytes, mode_t accessPermissions,
                                  Fam_Region_Descriptor *region) {
    return pimpl_->fam_allocate(nbytes, accessPermissions, region);
}
Fam_Descriptor *fam::fam_allocate(const char *name, uint64_t nbytes,
                                  mode_t accessPermissions,
                                  Fam_Region_Descriptor *region) {
    return pimpl_->fam_allocate(name, nbytes, accessPermissions, region);
}

/**
 * Deallocate allocated space in memory
 * @param descriptor - descriptor associated with the space.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_allocate()
 */
void fam::fam_deallocate(Fam_Descriptor *descriptor) {
    pimpl_->fam_deallocate(descriptor);
}

/**
 * Change permissions associated with a data item descriptor.
 * @param descriptor - descriptor associated with some data item
 * @param accessPermissions - new permissions for the data item
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 */
int fam::fam_change_permissions(Fam_Descriptor *descriptor,
                                mode_t accessPermissions) {
    return pimpl_->fam_change_permissions(descriptor, accessPermissions);
}

/**
 * Change permissions associated with a region descriptor.
 * @param descriptor - descriptor associated with some region
 * @param accessPermissions - new permissions for the region
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 on success, 1 for unsuccessful completion, negative number on an
 * exception
 */
int fam::fam_change_permissions(Fam_Region_Descriptor *descriptor,
                                mode_t accessPermissions) {
    return pimpl_->fam_change_permissions(descriptor, accessPermissions);
}

/**
 * Get the size, permissions and name of the data item associated with
 * a data item descriptor.
 * @param descriptor - descriptor associated with some data item.
 * @param famInfo - Structure that holds the size, permission and
 * name info for the specified data item descriptor.
 * @return - none
 */

void fam::fam_stat(Fam_Descriptor *descriptor, Fam_Stat *famInfo) {
    return pimpl_->fam_stat(descriptor, famInfo);
}

/**
 * Get the size, permissions and name of the region associated
 * with a region descriptor.
 * @param descriptor - descriptor associated with some region.
 * @param famInfo - Structure that holds the size, permission and
 * name info for the specified region descriptor.
 * @return - none
 */
void fam::fam_stat(Fam_Region_Descriptor *descriptor, Fam_Stat *famInfo) {
    return pimpl_->fam_stat(descriptor, famInfo);
}

// DATA READ AND WRITE Group. These APIs read and write data in FAM and copy
// data between local DRAM and FAM.

// DATA GET/PUT sub-group

/**
 * Copy data from FAM to node local memory, blocking the caller while the copy
 * is completed.
 * @param descriptor - valid descriptor to area in FAM.
 * @param local - pointer to local memory region where data needs to be copied.
 * Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor from
 * where memory should be copied
 * @param nbytes - number of bytes to be copied from global to local memory
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for successful completion, 1 for unsuccessful, and a negative
 * number in case of exceptions
 */
int fam::fam_get_blocking(void *local, Fam_Descriptor *descriptor,
                          uint64_t offset, uint64_t nbytes) {
    return pimpl_->fam_get_blocking(local, descriptor, offset, nbytes);
}

/**
 * Initiate a copy of data from FAM to node local memory. Do not wait until copy
 * is finished
 * @param descriptor - valid descriptor to area in FAM.
 * @param local - pointer to local memory region where data needs to be copied.
 * Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor from
 * where memory should be copied
 * @param nbytes - number of bytes to be copied from global to local memory
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_get_nonblocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t offset, uint64_t nbytes) {
    pimpl_->fam_get_nonblocking(local, descriptor, offset, nbytes);
}

/**
 * Copy data from local memory to FAM, blocking until the copy is complete.
 * @param local - pointer to local memory. Must point to valid data in local
 * memory
 * @param descriptor - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor to
 * where data should be copied
 * @param nbytes - number of bytes to be copied from local to FAM
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for successful completion, 1 for unsuccessful completion,
 * negative number in case of exceptions
 */
int fam::fam_put_blocking(void *local, Fam_Descriptor *descriptor,
                          uint64_t offset, uint64_t nbytes) {
    return pimpl_->fam_put_blocking(local, descriptor, offset, nbytes);
}

/**
 * Initiate a copy of data from local memory to FAM, returning before copy is
 * complete
 * @param local - pointer to local memory. Must point to valid data in local
 * memory
 * @param descriptor - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor to
 * where data should be copied
 * @param nbytes - number of bytes to be copied from local to FAM
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_put_nonblocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t offset, uint64_t nbytes) {
    pimpl_->fam_put_nonblocking(local, descriptor, offset, nbytes);
}

// LOAD/STORE sub-group

/**
 * Map a data item in FAM to the local virtual address space, and return its
 * pointer.
 * @param descriptor - descriptor to be mapped
 * @return pointer within the process virtual address space that can be used to
 * directly access the data item in FAM
 * @see #fam_unmap()
 */
void *fam::fam_map(Fam_Descriptor *descriptor) {
    return pimpl_->fam_map(descriptor);
}

/**
 * Unmap a data item in FAM from the local virtual address space.
 * @param local - pointer within the process virtual address space to be
 * unmapped
 * @param descriptor - descriptor for the FAM to be unmapped
 * @see #fam_map()
 */
void fam::fam_unmap(void *local, Fam_Descriptor *descriptor) {
    pimpl_->fam_unmap(local, descriptor);
}

// GATHER/SCATTER subgroup

/**
 * Gather data from FAM to local memory, blocking while the copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param firstElement - first element in FAM to include in the strided access
 * @param stride - stride in elements
 * @param elementSize - size of the element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case of exception
 * @see #fam_scatter_strided
 */
int fam::fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t firstElement,
                             uint64_t stride, uint64_t elementSize) {
    return pimpl_->fam_gather_blocking(local, descriptor, nElements,
                                       firstElement, stride, elementSize);
}

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param elementIndex - array of element indexes in FAM to fetch
 * @param elementSize - size of each element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_scatter_indexed
 */
int fam::fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t *elementIndex,
                             uint64_t elementSize) {
    return pimpl_->fam_gather_blocking(local, descriptor, nElements,
                                       elementIndex, elementSize);
}

/**
 * Initiate a gather of data from FAM to local memory, return before completion
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param firstElement - first element in FAM to include in the strided access
 * @param stride - stride in elements
 * @param elementSize - size of the element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_scatter_strided
 */
void fam::fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t firstElement,
                                 uint64_t stride, uint64_t elementSize) {
    pimpl_->fam_gather_nonblocking(local, descriptor, nElements, firstElement,
                                   stride, elementSize);
}

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array in
 * local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from multiple
 * data items.
 * @param local - pointer to local memory array. Must be large enough to contain
 * returned data
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be gathered in local memory
 * @param elementIndex - array of element indexes in FAM to fetch
 * @param elementSize - size of each element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @see #fam_scatter_indexed
 */
void fam::fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t *elementIndex,
                                 uint64_t elementSize) {
    pimpl_->fam_gather_nonblocking(local, descriptor, nElements, elementIndex,
                                   elementSize);
}

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param firstElement - placement of the first element in FAM to place for the
 * strided access
 * @param stride - stride in elements
 * @param elementSize - size of each element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_strided
 */
int fam::fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t nElements, uint64_t firstElement,
                              uint64_t stride, uint64_t elementSize) {
    return pimpl_->fam_scatter_blocking(local, descriptor, nElements,
                                        firstElement, stride, elementSize);
}

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing data elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param elementIndex - array containing element indexes
 * @param elementSize - size of the element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_indexed
 */
int fam::fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t nElements, uint64_t *elementIndex,
                              uint64_t elementSize) {
    return pimpl_->fam_scatter_blocking(local, descriptor, nElements,
                                        elementIndex, elementSize);
}

/**
 * initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param firstElement - placement of the first element in FAM to place for the
 * strided access
 * @param stride - stride in elements
 * @param elementSize - size of each element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_strided
 */
void fam::fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t nElements, uint64_t firstElement,
                                  uint64_t stride, uint64_t elementSize) {
    pimpl_->fam_scatter_nonblocking(local, descriptor, nElements, firstElement,
                                    stride, elementSize);
}

/**
 * Initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint elements of
 * a data item in FAM. Currently constrained to scatter data to a single FAM
 * descriptor, but can be extended if data needs to be scattered to multiple
 * data items.
 * @param local - pointer to local memory region containing data elements
 * @param descriptor - valid descriptor containing FAM reference
 * @param nElements - number of elements to be scattered from local memory
 * @param elementIndex - array containing element indexes
 * @param elementSize - size of the element in bytes
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - 0 for normal completion, 1 in case of unsuccessful completion,
 * negative number in case errors
 * @see #fam_gather_indexed
 */
void fam::fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                  uint64_t nElements, uint64_t *elementIndex,
                                  uint64_t elementSize) {
    pimpl_->fam_scatter_nonblocking(local, descriptor, nElements, elementIndex,
                                    elementSize);
}

// COPY Subgroup

/**
 * Copy data from one FAM-resident data item to another FAM-resident data item
 * (potentially within a different region).
 * @param src - valid descriptor to source data item in FAM.
 * @param srcOffset - byte offset within the space defined by the src descriptor
 * from which memory should be copied.
 * @param dest - valid descriptor to destination data item in FAM.
 * @param destOffset - byte offset within the space defined by the dest
 * descriptor to which memory should be copied.
 * @param nbytes - number of bytes to be copied
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_OUTOFRANGE, FAM_ERR_RPC
 *
 * Note : In case of copy operation across memoryserver this API is blocking
 * and no need to wait on copy.
 */
void *fam::fam_copy(Fam_Descriptor *src, uint64_t srcOffset,
                    Fam_Descriptor *dest, uint64_t destOffset,
                    uint64_t nbytes) {
    return pimpl_->fam_copy(src, srcOffset, dest, destOffset, nbytes);
}

void fam::fam_copy_wait(void *waitObj) { pimpl_->fam_copy_wait(waitObj); }

// ATOMICS Group

// NON fetching routines

/**
 * set group - atomically set a value at a given offset within a data item in
 * FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be set at the given location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, int32_t value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, int64_t value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, int128_t value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, float value) {
    pimpl_->fam_set(descriptor, offset, value);
}
void fam::fam_set(Fam_Descriptor *descriptor, uint64_t offset, double value) {
    pimpl_->fam_set(descriptor, offset, value);
}

/**
 * add group - atomically add a value to the value at a given offset within a
 * data item in FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be added to the existing value at the given location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, int32_t value) {
    pimpl_->fam_add(descriptor, offset, value);
}
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, int64_t value) {
    pimpl_->fam_add(descriptor, offset, value);
}
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_add(descriptor, offset, value);
}
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_add(descriptor, offset, value);
}
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, float value) {
    pimpl_->fam_add(descriptor, offset, value);
}
void fam::fam_add(Fam_Descriptor *descriptor, uint64_t offset, double value) {
    pimpl_->fam_add(descriptor, offset, value);
}

/**
 * subtract group - atomically subtract a value from a value at a given offset
 * within a data item in FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be subtracted from the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       int32_t value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       int64_t value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       uint32_t value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       uint64_t value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       float value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}
void fam::fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                       double value) {
    pimpl_->fam_subtract(descriptor, offset, value);
}

/**
 * min group - atomically set the value at a given offset within a data item in
 * FAM to the smaller of the existing value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared to the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, int32_t value) {
    pimpl_->fam_min(descriptor, offset, value);
}
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, int64_t value) {
    pimpl_->fam_min(descriptor, offset, value);
}
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_min(descriptor, offset, value);
}
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_min(descriptor, offset, value);
}
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, float value) {
    pimpl_->fam_min(descriptor, offset, value);
}
void fam::fam_min(Fam_Descriptor *descriptor, uint64_t offset, double value) {
    pimpl_->fam_min(descriptor, offset, value);
}

/**
 * max group - atomically set the value at a given offset within a data item in
 * FAM to the larger of the value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared to the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, int32_t value) {
    pimpl_->fam_max(descriptor, offset, value);
}
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, int64_t value) {
    pimpl_->fam_max(descriptor, offset, value);
}
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_max(descriptor, offset, value);
}
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_max(descriptor, offset, value);
}
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, float value) {
    pimpl_->fam_max(descriptor, offset, value);
}
void fam::fam_max(Fam_Descriptor *descriptor, uint64_t offset, double value) {
    pimpl_->fam_max(descriptor, offset, value);
}

/**
 * and group - atomically replace the value at a given offset within a data item
 * in FAM with the logical AND of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_and(descriptor, offset, value);
}
void fam::fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_and(descriptor, offset, value);
}

/**
 * or group - atomically replace the value at a given offset within a data item
 * in FAM with the logical OR of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_or(descriptor, offset, value);
}
void fam::fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_or(descriptor, offset, value);
}

/**
 * xor group - atomically replace the value at a given offset within a data item
 * in FAM with the logical XOR of that value and the given value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 */
void fam::fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value) {
    pimpl_->fam_xor(descriptor, offset, value);
}
void fam::fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value) {
    pimpl_->fam_xor(descriptor, offset, value);
}

// FETCHING Routines - perform the operation, and return the old value in FAM

/**
 * fetch group - atomically fetches the value at the given offset within a data
 * item from FAM
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - value from the given location in FAM
 */
int32_t fam::fam_fetch_int32(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_int32(descriptor, offset);
}
int64_t fam::fam_fetch_int64(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_int64(descriptor, offset);
}
int128_t fam::fam_fetch_int128(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_int128(descriptor, offset);
}
uint32_t fam::fam_fetch_uint32(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_uint32(descriptor, offset);
}
uint64_t fam::fam_fetch_uint64(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_uint64(descriptor, offset);
}
float fam::fam_fetch_float(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_float(descriptor, offset);
}
double fam::fam_fetch_double(Fam_Descriptor *descriptor, uint64_t offset) {
    return pimpl_->fam_fetch_double(descriptor, offset);
}

/**
 * swap group - atomically replace the value at the given offset within a data
 * item in FAM with the given value, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be swapped with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      int32_t value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}
int64_t fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      int64_t value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}
uint32_t fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                       uint32_t value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}
uint64_t fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                       uint64_t value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}
float fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset, float value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}
double fam::fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                     double value) {
    return pimpl_->fam_swap(descriptor, offset, value);
}

/**
 * compare and swap group - atomically conditionally replace the value at the
 * given offset within a data item in FAM with the given value, and return the
 * old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param oldValue - value to be compared with the existing value at the given
 * location
 * @param newValue - new value to be stored if comparison is successful
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              int32_t oldValue, int32_t newValue) {
    return pimpl_->fam_compare_swap(descriptor, offset, oldValue, newValue);
}
int64_t fam::fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              int64_t oldValue, int64_t newValue) {
    return pimpl_->fam_compare_swap(descriptor, offset, oldValue, newValue);
}
uint32_t fam::fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                               uint32_t oldValue, uint32_t newValue) {
    return pimpl_->fam_compare_swap(descriptor, offset, oldValue, newValue);
}
uint64_t fam::fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                               uint64_t oldValue, uint64_t newValue) {
    return pimpl_->fam_compare_swap(descriptor, offset, oldValue, newValue);
}
int128_t fam::fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                               int128_t oldValue, int128_t newValue) {
    return pimpl_->fam_compare_swap(descriptor, offset, oldValue, newValue);
}

/**
 * fetch and add group - atomically add the given value to the value at the
 * given offset within a data item in FAM, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be added to the existing value at the given location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           int32_t value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}
int64_t fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           int64_t value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}
uint32_t fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}
uint64_t fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}
float fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}
double fam::fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                          double value) {
    return pimpl_->fam_fetch_add(descriptor, offset, value);
}

/**
 * fetch and subtract group - atomically subtract the given value from the value
 * at the given offset within a data item in FAM, and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be subtracted from the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                int32_t value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}
int64_t fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                int64_t value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}
uint32_t fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint32_t value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}
uint64_t fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint64_t value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}
float fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              float value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}
double fam::fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                               double value) {
    return pimpl_->fam_fetch_subtract(descriptor, offset, value);
}

/**
 * fetch and min group - atomically set the value at a given offset within a
 * data item in FAM to the smaller of the existing value and the given value,
 * and return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           int32_t value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}
int64_t fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           int64_t value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}
uint32_t fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}
uint64_t fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}
float fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}
double fam::fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                          double value) {
    return pimpl_->fam_fetch_min(descriptor, offset, value);
}

/**
 * fetch and max group - atomically set the value at a given offset within a
 * data item in FAM to the larger of the existing value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
int32_t fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           int32_t value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}
int64_t fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           int64_t value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}
uint32_t fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}
uint64_t fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}
float fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                         float value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}
double fam::fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                          double value) {
    return pimpl_->fam_fetch_max(descriptor, offset, value);
}

/**
 * fetch and and group - atomically replace the value at a given offset within a
 * data item in FAM with the logical AND of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
uint32_t fam::fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    return pimpl_->fam_fetch_and(descriptor, offset, value);
}
uint64_t fam::fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    return pimpl_->fam_fetch_and(descriptor, offset, value);
}

/**
 * fetch and or group - atomically replace the value at a given offset within a
 * data item in FAM with the logical OR of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
uint32_t fam::fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value) {
    return pimpl_->fam_fetch_or(descriptor, offset, value);
}
uint64_t fam::fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value) {
    return pimpl_->fam_fetch_or(descriptor, offset, value);
}

/**
 * fetch and xor group - atomically replace the value at a given offset within a
 * data item in FAM with the logical XOR of that value and the given value, and
 * return the old value
 * @param descriptor - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @throws Fam_InvalidOption_Exception.
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 * @throws Fam_Allocator_Exception - exceptionObj->fam_error() may return:
 *         FAM_ERR_NOPERM, FAM_ERR_NOTFOUND, FAM_ERR_RPC
 * @return - old value from the given location in FAM
 */
uint32_t fam::fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) {
    return pimpl_->fam_fetch_xor(descriptor, offset, value);
}
uint64_t fam::fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) {
    return pimpl_->fam_fetch_xor(descriptor, offset, value);
}

// MEMORY ORDERING Routines - provide ordering of FAM operations issued by a PE

/**
 * fam_fence - ensures that FAM operations (put, scatter, atomics, copy) issued
 * by the calling PE thread before the fence are ordered before FAM operations
 * issued by the calling thread after the fence Note that method this does NOT
 * order load/store accesses by the processor to FAM enabled by fam_map().
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 */
void fam::fam_fence() { pimpl_->fam_fence(); }

/**
 * fam_quiet - blocks the calling PE thread until all its pending FAM operations
 * (put, scatter, atomics, copy) are completed. Note that method this does NOT
 * order or wait for completion of load/store accesses by the processor to FAM
 * enabled by fam_map().
 * @throws Fam_Datapath_Exception.
 * @throws Fam_Timeout_Exception.
 */
void fam::fam_quiet() { pimpl_->fam_quiet(); }

/**
 * fam() - constructor for fam class
 */
fam::fam() { pimpl_ = new Impl_; }

/**
 * ~fam() - destructor for fam class
 */
fam::~fam() {
    if (pimpl_)
        delete pimpl_;
}

} // namespace openfam

/*
 * fam_microbenchmark.cpp
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

#include <boost/atomic.hpp>
#include <chrono>
#include <iomanip>
#include <string.h>
#include <unistd.h>

using namespace std;
using namespace chrono;

typedef __attribute__((unused)) uint64_t Profile_Time;

#ifdef MEMSERVER_PROFILE

using Memserver_Time = boost::atomic_uint64_t;

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) prof_##name,
typedef enum NVMM_Counter_Enum {
#include "allocator/NVMM_counters.tbl"
    NVMM_COUNTER_MAX
} NVMM_Counter_Enum_T;

typedef enum Rpc_Service_Counter_Enum {
#include "rpc/rpc_service_counters.tbl"
    RPC_SERVICE_COUNTER_MAX
} Rpc_Service_Counter_Enum_T;

typedef enum Rpc_Server_Counter_Enum {
#include "rpc/rpc_server_counters.tbl"
    RPC_SERVER_COUNTER_MAX
} Rpc_Server_Counter_Enum_T;

typedef enum Metadata_Counter_Enum {
#include "metadata/metadata_counters.tbl"
    METADATA_COUNTER_MAX
} Metadata_Counter_Enum_T;

#define MEMSERVER_PROFILE_START(PROFILE_NAME)                                  \
    struct PROFILE_NAME##_Counter_St {                                         \
        Memserver_Time count;                                                  \
        Memserver_Time start;                                                  \
        Memserver_Time end;                                                    \
        Memserver_Time total;                                                  \
    };                                                                         \
    PROFILE_NAME##_Counter_St                                                  \
        profile##PROFILE_NAME##Data[PROFILE_NAME##_COUNTER_MAX];               \
    uint64_t PROFILE_NAME##_profile_time;                                      \
    uint64_t PROFILE_NAME##_profile_start;                                     \
    uint64_t PROFILE_NAME##_lib_time = 0;                                      \
    uint64_t PROFILE_NAME##_ops_time = 0;                                      \
    uint64_t PROFILE_NAME##_get_time() {                                       \
        long int time = static_cast<long int>(                                 \
            duration_cast<nanoseconds>(                                        \
                high_resolution_clock::now().time_since_epoch())               \
                .count());                                                     \
        return time;                                                           \
    }                                                                          \
    uint64_t PROFILE_NAME##_time_diff_nanoseconds(Profile_Time start,          \
                                                  Profile_Time end) {          \
        return (end - start);                                                  \
    }

#define OUTPUT_WIDTH 140
#define ITEM_WIDTH OUTPUT_WIDTH / 5

#define MEMSERVER_PROFILE_START_TIME(PROFILE_NAME)                             \
    PROFILE_NAME##_profile_start = PROFILE_NAME##_get_time();

#define MEMSERVER_PROFILE_END(PROFILE_NAME)                                    \
    {                                                                          \
        PROFILE_NAME##_profile_time =                                          \
            PROFILE_NAME##_get_time() - PROFILE_NAME##_profile_start;          \
    }

#define MEMSERVER_PROFILE_INIT(PROFILE_NAME)                                   \
    {                                                                          \
        memset(profile##PROFILE_NAME##Data, 0,                                 \
               sizeof(profile##PROFILE_NAME##Data));                           \
    }

#define MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(PROFILE_NAME, apiIdx, func_time)    \
    {                                                                          \
        uint64_t one = 1;                                                      \
        profile##PROFILE_NAME##Data[apiIdx].total.fetch_add(                   \
            func_time, boost::memory_order_seq_cst);                           \
        profile##PROFILE_NAME##Data[apiIdx].count.fetch_add(                   \
            one, boost::memory_order_seq_cst);                                 \
    }

#define DUMP_HEADING1(name)                                                    \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << name;

#define DUMP_HEADING2(name)                                                    \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH)                      \
         << string(strlen(name), '-');

#define MEMSERVER_DUMP_PROFILE_BANNER(PROFILE_NAME)                            \
    {                                                                          \
        {                                                                      \
            string header = #PROFILE_NAME " PROFILE DATA";                     \
            cout << endl;                                                      \
            cout << setfill('-') << setw(OUTPUT_WIDTH) << "-" << endl;         \
            cout << setfill(' ')                                               \
                 << setw((int)(OUTPUT_WIDTH - header.length()) / 2) << " ";    \
            cout << #PROFILE_NAME << " PROFILE DATA";                          \
            cout << setfill(' ')                                               \
                 << setw((int)(OUTPUT_WIDTH - header.length()) / 2) << " "     \
                 << endl;                                                      \
            cout << setfill('-') << setw(OUTPUT_WIDTH) << "-" << endl;         \
        }                                                                      \
        DUMP_HEADING1("Function");                                             \
        DUMP_HEADING1("Count");                                                \
        DUMP_HEADING1("Total Pct");                                            \
        DUMP_HEADING1("Total time(ns)");                                       \
        DUMP_HEADING1("Avg time/call(ns)");                                    \
        cout << endl;                                                          \
        DUMP_HEADING2("Function");                                             \
        DUMP_HEADING2("Count");                                                \
        DUMP_HEADING2("Total Pct");                                            \
        DUMP_HEADING2("Total time(ns)");                                       \
        DUMP_HEADING2("Avg time/call(ns)");                                    \
        cout << endl;                                                          \
    }

#define MEMSERVER_DUMP_PROFILE_DATA(profile_name, name, apiIdx)                \
    if (profile##profile_name##Data[apiIdx].count) {                           \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << #name;        \
        cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)   \
             << profile##profile_name##Data[apiIdx].count;                     \
                                                                               \
        double time_pct =                                                      \
            (double)((double)profile##profile_name##Data[apiIdx].total * 100 / \
                     (double)profile_name##_profile_time);                     \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << std::fixed    \
             << setprecision(2) << time_pct;                                   \
        cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)   \
             << profile##profile_name##Data[apiIdx].total;                     \
                                                                               \
        uint64_t avg_time = (profile##profile_name##Data[apiIdx].total /       \
                             profile##profile_name##Data[apiIdx].count);       \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << avg_time;     \
        cout << endl;                                                          \
    }

#define MEMSERVER_PROFILE_TOTAL(profile_name, apiIdx)                          \
    { profile_name##_ops_time += profile##profile_name##Data[apiIdx].total; }

#define MEMSERVER_SUMMARY_HEADING()                                            \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << "Summary"         \
         << endl;                                                              \
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH)                      \
         << string(strlen("Summary"), '-') << endl;

#define MEMSERVER_SUMMARY_ENTRY(profile_name, name, value)                     \
    cout << std::left << std::fixed << setprecision(2) << setfill(' ')         \
         << setw(ITEM_WIDTH) << name << setw(10) << ":" << value << " ns ("    \
         << value * 100 / profile_name##_profile_time << "%)" << endl;

#define MEMSERVER_DUMP_PROFILE_SUMMARY(profile_name)                           \
    {                                                                          \
        cout << endl;                                                          \
        MEMSERVER_SUMMARY_HEADING();                                           \
        MEMSERVER_SUMMARY_ENTRY(profile_name, "Total time",                    \
                                profile_name##_ops_time);                      \
        cout << endl;                                                          \
    }

#else
#define MEMSERVER_PROFILE_DECLARE(PROFILE_NAME)
#define MEMSERVER_PROFILE_GET_TIME_FUNC(PROFILE_NAME)                          \
    uint64_t PROFILE_NAME##_get_time() { return 0; }
#define MEMSERVER_PROFILE_TIME_DIFF_NS_FUNC(PROFILE_NAME)                      \
    uint64_t PROFILE_NAME##_time_diff_nanoseconds(Profile_Time start,          \
                                                  Profile_Time end) {          \
        return 0;                                                              \
    }
#define MEMSERVER_PROFILE_START(PROFILE_NAME)
#define MEMSERVER_PROFILE_START_TIME(PROFILE_NAME)
#define MEMSERVER_PROFILE_END(PROFILE_NAME)
#define MEMSERVER_PROFILE_INIT(PROFILE_NAME)
#define MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(PROFILE_NAME, apiIdx, func_time)
#define MEMSERVER_DUMP_PROFILE_BANNER(PROFILE_NAME)
#define MEMSERVER_DUMP_PROFILE_DATA(profile_name, name, apiIdx)
#define MEMSERVER_PROFILE_TOTAL(profile_name, apiIdx)
#define MEMSERVER_DUMP_PROFILE_SUMMARY(profile_name)
#endif

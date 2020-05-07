/*
 * fam_libfabric.cpp
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

#include "common/fam_libfabric.h"
#include "common/fam_context.h"
#include "common/fam_internal.h"
#include "common/fam_options.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "string.h"
#include <atomic>
#include <boost/atomic.hpp>
#include <chrono>
#include <iomanip>
#include <limits.h>
#include <list>
#include <unistd.h>

using namespace std;
using namespace chrono;

using namespace std;

#define MAX_RETRY_CNT INT_MAX
#define FABRIC_TIMEOUT 10     // 10 milliseconds
#define TOTAL_TIMEOUT 3600000 // 1 hour
#define TIMEOUT_WAIT_RETRY (TOTAL_TIMEOUT / FABRIC_TIMEOUT)
#define TIMEOUT_RETRY INT_MAX

namespace openfam {

typedef __attribute__((unused)) uint64_t Profile_Time;

#ifdef LIBFABRIC_PROFILE
using LibFabric_Time = boost::atomic_uint64_t;

struct LibFabric_Counter_St {
    LibFabric_Time count;
    LibFabric_Time start;
    LibFabric_Time end;
    LibFabric_Time total;
};
typedef enum LibFabric_Counter_Enum {
#undef LIBFABRIC_COUNTER
#define LIBFABRIC_COUNTER(name) prof_##name,
#include "libfabric_counters.tbl"
    libfabric_counter_max
} Libfabric_Counter_Enum_T;
#define OUTPUT_WIDTH 140
#define ITEM_WIDTH OUTPUT_WIDTH / 5
uint64_t fabric_profile_time;
uint64_t fabric_profile_start;
uint64_t libfabric_lib_time = 0;
uint64_t libfabric_ops_time = 0;

LibFabric_Counter_St profileLibfabricData[libfabric_counter_max];

uint64_t libfabric_get_time() {
    long int time = static_cast<long int>(
        duration_cast<nanoseconds>(
            high_resolution_clock::now().time_since_epoch())
            .count());
    return time;
}

uint64_t libfabric_time_diff_nanoseconds(Profile_Time start, Profile_Time end) {
    return (end - start);
}
void libfabric_total_api_time(int apiIdx) {}
#define LIBFABRIC_PROFILE_START_TIME()                                         \
    fabric_profile_start = libfabric_get_time();
#define LIBFABRIC_PROFILE_INIT() libfabric_profile_init();
#define LIBFABRIC_PROFILE_END()                                                \
    {                                                                          \
        fabric_profile_time = libfabric_get_time() - fabric_profile_start;     \
        libfabric_dump_profile_banner();                                       \
        libfabric_dump_profile_data();                                         \
        libfabric_dump_profile_summary();                                      \
    }

#define LIBFABRIC_PROFILE_START_OPS()                                          \
    {                                                                          \
        Profile_Time start = libfabric_get_time();

#define LIBFABRIC_PROFILE_END_OPS(apiIdx)                                      \
    Profile_Time end = libfabric_get_time();                                   \
    Profile_Time total = libfabric_time_diff_nanoseconds(start, end);          \
    libfabric_add_to_total_profile(prof_##apiIdx, total);                      \
    }

void libfabric_profile_init() {
    memset(profileLibfabricData, 0, sizeof(profileLibfabricData));
}
void libfabric_start_profile(int apiIdx) {
    profileLibfabricData[apiIdx].start = libfabric_get_time();
}

void libfabric_add_to_total_profile(int apiIdx, Profile_Time total) {
    uint64_t one = 1;
    profileLibfabricData[apiIdx].total.fetch_add(total,
                                                 boost::memory_order_seq_cst);
    profileLibfabricData[apiIdx].count.fetch_add(one,
                                                 boost::memory_order_seq_cst);
}
void libfabric_end_profile(int apiIdx) {
    profileLibfabricData[apiIdx].end = libfabric_get_time();
    profileLibfabricData[apiIdx].total += libfabric_time_diff_nanoseconds(
        profileLibfabricData[apiIdx].start, profileLibfabricData[apiIdx].end);
    profileLibfabricData[apiIdx].count += 1;
}

void libfabric_dump_profile_banner(void) {
    {
        string header = "LIBFABRIC PROFILE DATA";
        cout << endl;
        cout << setfill('-') << setw(OUTPUT_WIDTH) << "-" << endl;
        cout << setfill(' ') << setw((int)(OUTPUT_WIDTH - header.length()) / 2)
             << " ";
        cout << "LIBFABRIC PROFILE DATA";
        cout << setfill(' ') << setw((int)(OUTPUT_WIDTH - header.length()) / 2)
             << " " << endl;
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

void libfabric_dump_profile_data(void) {
#define DUMP_DATA_COUNT(idx)                                                   \
    cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)       \
         << profileLibfabricData[idx].count;

#define DUMP_DATA_TIME(idx)                                                    \
    cout << std::left << setbase(10) << setfill(' ') << setw(ITEM_WIDTH)       \
         << profileLibfabricData[idx].total;

#define DUMP_DATA_PCT(idx)                                                     \
    {                                                                          \
        double time_pct = (double)((double)profileLibfabricData[idx].total *   \
                                   100 / (double)fabric_profile_time);         \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << std::fixed    \
             << setprecision(2) << time_pct;                                   \
    }

#define DUMP_DATA_AVG(idx)                                                     \
    {                                                                          \
        uint64_t avg_time = (profileLibfabricData[idx].total /                 \
                             profileLibfabricData[idx].count);                 \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << avg_time;     \
    }

#undef LIBFABRIC_COUNTER
#undef LIBFABRIC_COUNTER
#define LIBFABRIC_COUNTER(name) __LIBFABRIC_COUNTER(name, prof_##name)
#define __LIBFABRIC_COUNTER(name, apiIdx)                                      \
    if (profileLibfabricData[apiIdx].count) {                                  \
        cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << #name;        \
        DUMP_DATA_COUNT(apiIdx);                                               \
        DUMP_DATA_PCT(apiIdx);                                                 \
        DUMP_DATA_TIME(apiIdx);                                                \
        DUMP_DATA_AVG(apiIdx);                                                 \
        cout << endl;                                                          \
    }
#include "libfabric_counters.tbl"
    cout << endl;
}

void libfabric_dump_profile_summary(void) {
    uint64_t libfabric_ops_time = 0;
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH) << "Summary" << endl;
    cout << std::left << setfill(' ') << setw(ITEM_WIDTH)
         << string(strlen("Summary"), '-') << endl;

#define LIBFABRIC_SUMMARY_ENTRY(name, value)                                   \
    cout << std::left << std::fixed << setprecision(2) << setfill(' ')         \
         << setw(ITEM_WIDTH) << name << setw(10) << ":" << value << " ns ("    \
         << value * 100 / fabric_profile_time << "%)" << endl;
#undef LIBFABRIC_COUNTER
#undef __LIBFABRIC_COUNTER
#define LIBFABRIC_COUNTER(name) __LIBFABRIC_COUNTER(prof_##name)
#define __LIBFABRIC_COUNTER(apiIdx)                                            \
    { libfabric_ops_time += profileLibfabricData[apiIdx].total; }
#include "libfabric_counters.tbl"

    LIBFABRIC_SUMMARY_ENTRY("Total time", libfabric_ops_time);
    cout << endl;
}
#else

#define LIBFABRIC_PROFILE_START_OPS()
#define LIBFABRIC_PROFILE_END_OPS(apiIdx)
#define LIBFABRIC_PROFILE_START_TIME()
#define LIBFABRIC_PROFILE_INIT()
#define LIBFABRIC_PROFILE_END()
#endif

#define FI_CALL(retType, funcname, ...)                                        \
    {                                                                          \
        LIBFABRIC_PROFILE_START_OPS()                                          \
        retType = funcname(__VA_ARGS__);                                       \
        LIBFABRIC_PROFILE_END_OPS(funcname)                                    \
    }

#define FI_CALL_NO_RETURN(funcname, ...)                                       \
    {                                                                          \
        LIBFABRIC_PROFILE_START_OPS()                                          \
        funcname(__VA_ARGS__);                                                 \
        LIBFABRIC_PROFILE_END_OPS(funcname)                                    \
    }

/**
 * Initialize the libfabric library. This method is required to be the first
 * method called when a process uses the OpenFAM library.
 * @param name - name of the memory server
 * @param service - port
 * @param source -  to indicate if it is called by a memory node
 * @param provider - libfabric provider
 * @param fi - fi_info will be initialized
 * @param fabric - fi_fabric will be initialized
 * @param eq - fi_eq will be initialized
 * @param domain - fi_domain will be initialized
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_initialize(const char *name, const char *service, bool source,
                      char *provider, struct fi_info **fi,
                      struct fid_fabric **fabric, struct fid_eq **eq,
                      struct fid_domain **domain, Fam_Thread_Model famTM) {
    struct fi_info *hints;

    LIBFABRIC_PROFILE_INIT();
    LIBFABRIC_PROFILE_START_TIME();

    // Get the hints and initialize configuration
    FI_CALL(hints, fi_allocinfo);

    if (!hints) {
        return -1;
    }

    if (!hints->ep_attr)
        hints->ep_attr =
            (struct fi_ep_attr *)calloc(1, sizeof(struct fi_ep_attr));
    hints->ep_attr->type = FI_EP_RDM;

    if (!hints->tx_attr)
        hints->tx_attr =
            (struct fi_tx_attr *)calloc(1, sizeof(struct fi_tx_attr));
    hints->tx_attr->op_flags = FI_DELIVERY_COMPLETE;

    hints->caps = FI_MSG | FI_RMA | FI_ATOMICS;
    if ((strncmp(provider, "verbs", 5) != 0))
        hints->caps |= FI_RMA_EVENT;
    hints->mode = FI_CONTEXT;

    if (!hints->domain_attr)
        hints->domain_attr =
            (struct fi_domain_attr *)calloc(1, sizeof(struct fi_domain_attr));
    hints->domain_attr->av_type = FI_AV_MAP;

    if ((strncmp(provider, "verbs", 5) == 0)) {
        hints->domain_attr->mr_mode =
            FI_MR_ALLOCATED | FI_MR_PROV_KEY | FI_MR_VIRT_ADDR;
        if (!source)
            hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
    } else
        hints->domain_attr->mr_mode = FI_MR_SCALABLE;

    if (famTM == FAM_THREAD_SERIALIZE)
        hints->domain_attr->threading = FI_THREAD_DOMAIN;
    if (famTM == FAM_THREAD_MULTIPLE)
        hints->domain_attr->threading = FI_THREAD_SAFE;

    if (!hints->fabric_attr)
        hints->fabric_attr =
            (struct fi_fabric_attr *)calloc(1, sizeof(struct fi_fabric_attr));
    hints->fabric_attr->prov_name = strdup(provider);

    hints->domain_attr->resource_mgmt = FI_RM_ENABLED;

    int ret = 0;

    uint64_t flags = 0;
    // This flag is set only for memory service.
    if (source)
        flags |= FI_SOURCE;

    // Initialize fi with name and service(port)
    if ((strcmp(provider, "sockets") == 0) && (source)) {
        FI_CALL(ret, fi_getinfo, fi_version(), name, service, flags, hints, fi);
    } else {
        FI_CALL(ret, fi_getinfo, fi_version(), NULL, NULL, flags, hints, fi);
    }
    if (ret < 0) {
        // print_fierr("getinfo", ret);
        FI_CALL_NO_RETURN(fi_freeinfo, hints);
        return -1;
    }

    // Get the fabric_ initialized
    FI_CALL(ret, fi_fabric, (*fi)->fabric_attr, fabric, NULL);
    if (ret < 0) {
        // print_fierr("fi_fabric", ret);
        FI_CALL_NO_RETURN(fi_freeinfo, hints);
        FI_CALL_NO_RETURN(fi_freeinfo, *fi);
        *fi = NULL;
        return -1;
    }

    // Initialize  and open event queue
    struct fi_eq_attr eqAttr;
    memset(&eqAttr, 0, sizeof(eqAttr));
    eqAttr.wait_obj = FI_WAIT_UNSPEC;

    FI_CALL(ret, fi_eq_open, *fabric, &eqAttr, eq, NULL);
    if (ret < 0) {
        // print_fierr("fi_eq_open", ret);
        FI_CALL_NO_RETURN(fi_freeinfo, hints);
        FI_CALL_NO_RETURN(fi_freeinfo, *fi);
        *fi = NULL;
        FI_CALL_NO_RETURN(fi_close, &(*fabric)->fid);
        *fabric = NULL;
        return -1;
    }

    // Create a domain from fabric and fi
    FI_CALL(ret, fi_domain, *fabric, *fi, domain, NULL);
    if (ret < 0) {
        // print_fierr("fi_domain", ret);
        FI_CALL_NO_RETURN(fi_freeinfo, hints);
        FI_CALL_NO_RETURN(fi_freeinfo, *fi);
        *fi = NULL;
        FI_CALL_NO_RETURN(fi_close, &(*fabric)->fid);
        *fabric = NULL;
        FI_CALL_NO_RETURN(fi_close, &(*eq)->fid);
        *eq = NULL;
        return -1;
    }

    FI_CALL_NO_RETURN(fi_freeinfo, hints);
    return 0;
}
/**
 * Initialize and bind address vector
 * @param fi - struct fi_info
 * @param domein - struct fi_domain
 * @param eq - struct fi_eq
 * @param av - struct fid_av will be initialized
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_initialize_av(struct fi_info *fi, struct fid_domain *domain,
                         struct fid_eq *eq, struct fid_av **av) {
    int ret = 0;

    struct fi_av_attr avAttr;
    memset(&avAttr, 0, sizeof(avAttr));
    if (fi->domain_attr->av_type != FI_AV_UNSPEC)
        avAttr.type = fi->domain_attr->av_type;
    else
        avAttr.type = FI_AV_TABLE;
    FI_CALL(ret, fi_av_open, domain, &avAttr, av, NULL);
    if (ret < 0) {
        // print_fierr("fi_av_open", ret);
        return -1;
    }

    if (strncmp(fi->fabric_attr->prov_name, "verbs", 5) != 0)
        if (eq) {
            FI_CALL(ret, fi_av_bind, *av, &eq->fid, 0);
            if (ret < 0) {
                // print_fierr("fi_av_bind", ret);
                FI_CALL_NO_RETURN(fi_close, &(*av)->fid);
                *av = NULL;
                return -1;
            }
        }

    return 0;
}

/*
 * Insert address of the memory node into address vector.
 * @param addr - address of the memory server
 * @param av - struct fid_av
 * @param fiAddrs - vector of fi_addr_t
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_insert_av(const char *addr, struct fid_av *av,
                     std::vector<fi_addr_t> *fiAddrs) {
    fi_addr_t fiAddr;
    uint64_t flags = 0;
    void *context = 0;

    if (!fiAddrs)
        return -1;

    int num_success;
    FI_CALL(num_success, fi_av_insert, av, addr, 1, &fiAddr, flags, context);

    if (num_success < 1) {
        return -1;
    }

    fiAddrs->push_back(fiAddr);

    return 0;
}

/*
 * Enable and Bind endpoint
 * @param fi - struct fi_info
 * @param av - struct fid_av
 * @param eq - struct fid_eq
 * @param ep - struct fid_ep
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_enable_bind_ep(struct fi_info *fi, struct fid_av *av,
                          struct fid_eq *eq, struct fid_ep *ep) {
    int ret = 0;

    if (fi->ep_attr->type == FI_EP_MSG) {
        FI_CALL(ret, fi_ep_bind, ep, &eq->fid, 0);
        if (ret < 0) {
            // print_fierr("fi_ep_bind_eq", ret);
            return ret;
        }
    }

    if (av) {
        FI_CALL(ret, fi_ep_bind, ep, &av->fid, 0);
        if (ret < 0) {
            // print_fierr("fi_ep_bind", ret);
            return ret;
        }
    }

    FI_CALL(ret, fi_enable, ep);
    if (ret < 0) {
        // print_fierr("fi_enable_ep", ret);
        return ret;
    }

    return ret;
}

/*
 * Get server address name len
 * @param ep - struct fid_ep
 * @param addrSize - size_t to return name len
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_getname_len(struct fid_ep *ep, size_t *addrSize) {
    return (fi_getname(&ep->fid, NULL, addrSize));
}

/*
 * Get server address
 * @param ep - struct fid_ep
 * @param addr - void* buffer to return server addr
 * @param addrSize - size_t to be read as server addr
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_getname(struct fid_ep *ep, void *addr, size_t *addrSize) {
    return (fi_getname(&ep->fid, addr, addrSize));
}

/*
 * Register memory region
 * @param addr - local pointer of the memory region
 * @param size - Size of the memory region
 * @param key - key to be registered with the memory region
 * @param domain - struct fid_domain
 * @param fiMrs - Pointer to the map datastructure between key and fid_mr
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_register_mr(void *addr, size_t size, uint64_t *key,
                       struct fid_domain *domain, bool rw, fid_mr *&mr) {

    int ret = 0;
    uint64_t access = FI_RECV;
    access |= FI_READ;
    access |= FI_REMOTE_READ;

    if (rw) {
        access |= FI_SEND;
        access |= FI_WRITE;
        access |= FI_REMOTE_WRITE;
    }

    FI_CALL(ret, fi_mr_reg, domain, addr, size, access, 0, *key, 0, &mr, 0);
    if (ret < 0) {
        // print_fierr("fi_mr_reg", ret);
        return ret;
    }

    FI_CALL(*key, fi_mr_key, mr);
    return ret;
}

/*
 * Deregister memory region from fabric
 * @param key - key of the registerd memory region
 * @param fiMrs - Pointer to the map datastructure between key and fid_mr
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_deregister_mr(fid_mr *&mr) {
    FI_CALL_NO_RETURN(fi_close, &(mr->fid));
    return 0;
}

int fabric_retry(Fam_Context *famCtx, ssize_t ret, uint32_t *retry_cnt) {

    if (ret) {
        if (ret == -FI_EAGAIN) {
            struct fi_cq_err_entry err;
            FI_CALL(ret, fi_cq_readerr, famCtx->get_txcq(), &err, 0);
            if (ret == 1) {
                const char *errmsg = fi_cq_strerror(
                    famCtx->get_txcq(), err.prov_errno, err.err_data, NULL, 0);
                throw Fam_Datapath_Exception(errmsg);
            } else if (ret && ret != -FI_EAGAIN) {
                throw Fam_Datapath_Exception("Reading from fabric CQ failed");
            }

            (*retry_cnt)++;
            if ((*retry_cnt) <= MAX_RETRY_CNT) {
                return 1;
            } else {
                throw Fam_Datapath_Exception("Fabric max retry count exceeded");
            }
        } else {
            throw Fam_Datapath_Exception(fabric_strerror((int)ret));
        }
    }

    return 0;
}

int fabric_completion_wait(Fam_Context *famCtx, fi_context *ctx) {

    LIBFABRIC_PROFILE_START_OPS()
    ssize_t ret = 0;
    struct fi_cq_data_entry entry;
    int timeout_retry_cnt = 0;
    int timeout_wait_retry_cnt = 0;

    do {
        memset(&entry, 0, sizeof(entry));
        FI_CALL(ret, fi_cq_read, famCtx->get_txcq(), &entry, 1);
        if (ret == -FI_ETIMEDOUT || ret == -FI_EAGAIN) {
            if (timeout_retry_cnt < TIMEOUT_RETRY) {
                timeout_retry_cnt++;
                continue;
            } else if (timeout_wait_retry_cnt < TIMEOUT_WAIT_RETRY) {
                timeout_wait_retry_cnt++;
                usleep(FABRIC_TIMEOUT * 1000);
                continue;
            } else {
                throw Fam_Timeout_Exception(
                    "fi_cq_read timeout retry count exceeded INT_MAX");
            }
        }
        if (ret < 0) {
            struct fi_cq_err_entry err;

            FI_CALL(ret, fi_cq_readerr, famCtx->get_txcq(), &err, 0);

            if (ret == 1) {
                const char *errmsg = fi_cq_strerror(
                    famCtx->get_txcq(), err.prov_errno, err.err_data, NULL, 0);
                throw Fam_Datapath_Exception(get_fam_error(err.err), errmsg);
            } else if (ret && ret != -FI_EAGAIN) {
                throw Fam_Datapath_Exception("Reading from fabric CQ failed");
            }
        }
    } while (entry.op_context != (void *)ctx);

    LIBFABRIC_PROFILE_END_OPS(fabric_completion_wait)
    return 0;
}

int fabric_completion_wait_multictx(Fam_Context *famCtx, fi_context *ctx,
                                    int64_t count) {
    LIBFABRIC_PROFILE_START_OPS()
    ssize_t ret = 0;
    struct fi_cq_data_entry entry;
    int timeout_retry_cnt = 0;
    int64_t completion = 0;
    int timeout_wait_retry_cnt = 0;

    do {
        memset(&entry, 0, sizeof(entry));
        FI_CALL(ret, fi_cq_read, famCtx->get_txcq(), &entry, 1);
        if (ret == -FI_ETIMEDOUT || ret == -FI_EAGAIN) {
            if (timeout_retry_cnt < TIMEOUT_RETRY) {
                timeout_retry_cnt++;
                continue;
            } else if (timeout_wait_retry_cnt < TIMEOUT_WAIT_RETRY) {
                timeout_wait_retry_cnt++;
                usleep(FABRIC_TIMEOUT * 1000);
                continue;
            } else {
                throw Fam_Timeout_Exception(
                    "fi_cq_read timeout retry count exceeded INT_MAX");
            }
        }
        if (ret < 0) {
            struct fi_cq_err_entry err;

            FI_CALL(ret, fi_cq_readerr, famCtx->get_txcq(), &err, 0);

            if (ret == 1) {
                const char *errmsg = fi_cq_strerror(
                    famCtx->get_txcq(), err.prov_errno, err.err_data, NULL, 0);
                throw Fam_Datapath_Exception(get_fam_error(err.err), errmsg);
            } else if (ret == -FI_EAGAIN) {
                continue;
            } else if (ret) {
                throw Fam_Datapath_Exception("Reading from fabric CQ failed");
            }
        }

        // validate the completion context against the ctx at that index
        uint64_t idx = (uint64_t)((fi_context *)entry.op_context)->internal[0];
        if (&ctx[idx] == entry.op_context) {
            completion++;
        }
    } while (completion < count);

    LIBFABRIC_PROFILE_END_OPS(fabric_completion_wait_multictx)
    return 0;
}

/*
 * fabric write message blocking
 * @param key - key of the memory region
 * @param local - pointer to the local memory region
 * @param nbytes - number of the bytes to be written to memory region
 *                 registered with key
 * @param offset - offset to the local memory address
 * @param fiAddr - fi_addr_t address
 * @param famCtx - Pointer to Fam_Context
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_write(uint64_t key, const void *local, size_t nbytes,
                 uint64_t offset, fi_addr_t fiAddr, Fam_Context *famCtx) {

    struct iovec iov = {.iov_base = (void *)local, .iov_len = nbytes};

    struct fi_rma_iov rma_iov = {.addr = offset, .len = nbytes, .key = key};

    struct fi_context *ctx = new struct fi_context();
    struct fi_msg_rma msg = {.msg_iov = &iov,
                             .desc = 0,
                             .iov_count = 1,
                             .addr = fiAddr,
                             .rma_iov = &rma_iov,
                             .rma_iov_count = 1,
                             .context = ctx,
                             .data = 0};

    ssize_t ret;
    uint32_t retry_cnt = 0;
    uint64_t incr = 0;

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    try {
        do {
            FI_CALL(ret, fi_writemsg, famCtx->get_ep(), &msg,
                    FI_COMPLETION | FI_DELIVERY_COMPLETE);
        } while (fabric_retry(famCtx, ret, &retry_cnt));

        famCtx->inc_num_tx_ops();
        incr++;
        ret = fabric_completion_wait(famCtx, ctx);
    } catch (...) {
        famCtx->inc_num_tx_fail_cnt(incr);
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context read lock
    famCtx->release_lock();
    delete ctx;

    return (int)ret;
}

/*
 * Fabric read message blocking
 * @param key - key of the memory region
 * @param local - pointer to the local memory region
 * @param nbytes - number of the bytes to be read from memory region
 *                 registered with key
 * @param offset - offset to the local memory address
 * @param fiAddr - fi_addr_t address
 * @param famCtx - Pointer to Fam_Context
 * @return - {true(0), false(1), errNo(<0)}
 */
int fabric_read(uint64_t key, const void *local, size_t nbytes, uint64_t offset,
                fi_addr_t fiAddr, Fam_Context *famCtx) {

    struct iovec iov = {.iov_base = (void *)local, .iov_len = nbytes};

    struct fi_rma_iov rma_iov = {.addr = offset, .len = nbytes, .key = key};

    struct fi_context *ctx = new struct fi_context();
    struct fi_msg_rma msg = {.msg_iov = &iov,
                             .desc = 0,
                             .iov_count = 1,
                             .addr = fiAddr,
                             .rma_iov = &rma_iov,
                             .rma_iov_count = 1,
                             .context = ctx,
                             .data = 0};

    ssize_t ret;
    uint32_t retry_cnt = 0;
    uint64_t incr = 0;

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    try {
        do {
            FI_CALL(ret, fi_readmsg, famCtx->get_ep(), &msg, FI_COMPLETION);
        } while (fabric_retry(famCtx, ret, &retry_cnt));

        famCtx->inc_num_rx_ops();
        incr++;
        ret = fabric_completion_wait(famCtx, ctx);
    } catch (...) {
        famCtx->inc_num_rx_fail_cnt(incr);
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }
    delete ctx;

    return (int)ret;
}

int fabric_read_write_multi_msg(uint64_t count, size_t iov_limit,
                                fi_addr_t fiAddr, Fam_Context *famCtx,
                                struct iovec *iov, struct fi_rma_iov *rma_iov,
                                bool write, bool block) {

    int64_t iteration = count / iov_limit;
    if (count % iov_limit > 0)
        iteration++;

    int64_t count_remain = count;
    ssize_t ret = 0;
    uint64_t flags = 0;

    flags = (block ? FI_COMPLETION : 0);
    flags |= ((block && write) ? FI_DELIVERY_COMPLETE : 0);

    struct fi_context *ctx = (block ? new struct fi_context[iteration] : NULL);

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    for (int64_t j = 0; j < iteration; j++) {

        if (block)
            ctx[j].internal[0] = (void *)j;

        struct fi_msg_rma msg = {.msg_iov = &iov[j * iov_limit],
                                 .desc = 0,
                                 .iov_count = MIN(iov_limit, count_remain),
                                 .addr = fiAddr,
                                 .rma_iov = &rma_iov[j * iov_limit],
                                 .rma_iov_count = MIN(iov_limit, count_remain),
                                 .context = (block ? &ctx[j] : NULL),
                                 .data = 0};

        uint32_t retry_cnt = 0;

        try {
            do {
                if (write) {
                    FI_CALL(ret, fi_writemsg, famCtx->get_ep(), &msg, flags);
                } else {
                    FI_CALL(ret, fi_readmsg, famCtx->get_ep(), &msg, flags);
                }
            } while (fabric_retry(famCtx, ret, &retry_cnt));

            if (write)
                famCtx->inc_num_tx_ops();
            else
                famCtx->inc_num_rx_ops();
        } catch (...) {
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
        count_remain -= iov_limit;
    }

    if (block) {
        try {
            ret = fabric_completion_wait_multictx(famCtx, ctx, iteration);
        } catch (...) {
            if (write)
                famCtx->inc_num_tx_fail_cnt(1l);
            else
                famCtx->inc_num_rx_fail_cnt(1l);
            // Release Fam_Context read lock
            famCtx->release_lock();
            throw;
        }
    }
    // Release Fam_Context read lock
    famCtx->release_lock();

    if (block)
        delete[] ctx;
    return (int)ret;
}
/*
 *  fabric scatter stride message blocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be written to memory region
 *  registered with key
 *  @param first - offset of first element in FAM to place for the stride access
 *  @param count - number of elements to be scattered from local memory
 *  @param stride - stride size in element
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
int fabric_scatter_stride_blocking(uint64_t key, const void *local,
                                   size_t nbytes, uint64_t first,
                                   uint64_t count, uint64_t stride,
                                   fi_addr_t fiAddr, Fam_Context *famCtx,
                                   size_t iov_limit, uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + first * nbytes + (i * stride) * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    ret = fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov,
                                      rma_iov, 1, 1);
    delete iov;
    delete rma_iov;

    return ret;
}

/*
 *  Fabric gather stride blocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be read from memory region
 *  registered with key
 *  @param first - offset of first element in FAM to fetch for the stride access
 *  @param count - number of elements to be gathered to the local memory
 *  @param stride - stride size in element
 *  registered with key
 *  @param offset - offset to the local memory address
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */

int fabric_gather_stride_blocking(uint64_t key, const void *local,
                                  size_t nbytes, uint64_t first, uint64_t count,
                                  uint64_t stride, fi_addr_t fiAddr,
                                  Fam_Context *famCtx, size_t iov_limit,
                                  uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + first * nbytes + (i * stride) * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    ret = fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov,
                                      rma_iov, 0, 1);

    delete iov;
    delete rma_iov;

    return ret;
}

/*
 *  fabric scatter index blocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be written to memory region
 *  registered with key
 *  @param count - number of elements to be scattered from local memory
 *  @param index - An array containing element indexes.
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
int fabric_scatter_index_blocking(uint64_t key, const void *local,
                                  size_t nbytes, uint64_t *index,
                                  uint64_t count, fi_addr_t fiAddr,
                                  Fam_Context *famCtx, size_t iov_limit,
                                  uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;
        rma_iov[i].addr = base + index[i] * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    ret = fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov,
                                      rma_iov, 1, 1);

    delete iov;
    delete rma_iov;

    return ret;
}

/*
 *  Fabric gather index blocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be read from memory region
 *  registered with key
 *  @param count - number of elements to be gathered to the local memory
 *  @param index - An array containing element indexes.
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
int fabric_gather_index_blocking(uint64_t key, const void *local, size_t nbytes,
                                 uint64_t *index, uint64_t count,
                                 fi_addr_t fiAddr, Fam_Context *famCtx,
                                 size_t iov_limit, uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    int ret = 0;

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + index[i] * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    ret = fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov,
                                      rma_iov, 0, 1);

    delete iov;
    delete rma_iov;

    return ret;
}

/*
 * fabric write message nonblocking
 * @param key - key of the memory region
 * @param local - pointer to the local memory region
 * @param nbytes - number of the bytes to be written to memory region
 * registered with key
 * @param offset - offset to the local memory address
 * @param fiAddr - fi_addr_t address
 * @param famCtx - Pointer to Fam_Context
 * @return - {true(0), false(1), errNo(<0)}
 */
void fabric_write_nonblocking(uint64_t key, const void *local, size_t nbytes,
                              uint64_t offset, fi_addr_t fiAddr,
                              Fam_Context *famCtx) {

    struct iovec iov = {.iov_base = (void *)local, .iov_len = nbytes};

    struct fi_rma_iov rma_iov = {.addr = offset, .len = nbytes, .key = key};

    struct fi_msg_rma msg = {.msg_iov = &iov,
                             .desc = 0,
                             .iov_count = 1,
                             .addr = fiAddr,
                             .rma_iov = &rma_iov,
                             .rma_iov_count = 1,
                             .context = NULL,
                             .data = 0};

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    ssize_t ret;
    uint32_t retry_cnt = 0;

    try {
        do {
            FI_CALL(ret, fi_writemsg, famCtx->get_ep(), &msg, 0);
        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_tx_ops();
    } catch (...) {
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context read lock
    famCtx->release_lock();
    return;
}

/*
 *  Fabric read message nonblocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - number of the bytes to be read from memory region
 *  registered with key
 *  @param offset - offset to the local memory address
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @return - {true(0), false(1), errNo(<0)}
 */
void fabric_read_nonblocking(uint64_t key, const void *local, size_t nbytes,
                             uint64_t offset, fi_addr_t fiAddr,
                             Fam_Context *famCtx) {

    struct iovec iov = {.iov_base = (void *)local, .iov_len = nbytes};

    struct fi_rma_iov rma_iov = {.addr = offset, .len = nbytes, .key = key};

    struct fi_msg_rma msg = {.msg_iov = &iov,
                             .desc = 0,
                             .iov_count = 1,
                             .addr = fiAddr,
                             .rma_iov = &rma_iov,
                             .rma_iov_count = 1,
                             .context = NULL,
                             .data = 0};

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    ssize_t ret;
    uint32_t retry_cnt = 0;

    try {
        do {
            FI_CALL(ret, fi_readmsg, famCtx->get_ep(), &msg, 0);
        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_rx_ops();
    } catch (...) {
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }
    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

/*
 *  fabric scatter stride message nonblocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be written to memory region
 *  registered with key
 *  @param first - offset of first element in FAM to place for the stride access
 *  @param count - number of elements to be scattered from local memory
 *  @param stride - stride size in element
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
void fabric_scatter_stride_nonblocking(uint64_t key, const void *local,
                                       size_t nbytes, uint64_t first,
                                       uint64_t count, uint64_t stride,
                                       fi_addr_t fiAddr, Fam_Context *famCtx,
                                       size_t iov_limit, uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + first * nbytes + (i * stride) * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov, rma_iov,
                                1, 0);

    delete iov;
    delete rma_iov;

    return;
}

/*
 *  Fabric gather stride nonblocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be read from memory region
 *  registered with key
 *  @param first - offset of first element in FAM to fetch for the stride access
 *  @param count - number of elements to be gathered to the local memory
 *  @param stride - stride size in element
 *  registered with key
 *  @param offset - offset to the local memory address
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */

void fabric_gather_stride_nonblocking(uint64_t key, const void *local,
                                      size_t nbytes, uint64_t first,
                                      uint64_t count, uint64_t stride,
                                      fi_addr_t fiAddr, Fam_Context *famCtx,
                                      size_t iov_limit, uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + first * nbytes + (i * stride) * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov, rma_iov,
                                0, 0);

    delete iov;
    delete rma_iov;

    return;
}

/*
 *  fabric scatter index nonblocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be written to memory region
 *  registered with key
 *  @param count - number of elements to be scattered from local memory
 *  @param index - An array containing element indexes.
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
void fabric_scatter_index_nonblocking(uint64_t key, const void *local,
                                      size_t nbytes, uint64_t *index,
                                      uint64_t count, fi_addr_t fiAddr,
                                      Fam_Context *famCtx, size_t iov_limit,
                                      uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;
        rma_iov[i].addr = base + index[i] * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov, rma_iov,
                                1, 0);

    delete iov;
    delete rma_iov;

    return;
}

/*
 *  Fabric gather index nonblocking
 *  @param key - key of the memory region
 *  @param local - pointer to the local memory region
 *  @param nbytes - size of each element in bytes to be read from memory region
 *  registered with key
 *  @param count - number of elements to be gathered to the local memory
 *  @param index - An array containing element indexes.
 *  @param fiAddr - fi_addr_t address
 *  @param famCtx - Pointer to Fam_Context
 *  @param base - base address of remote memory
 *  @return - {true(0), false(1), errNo(<0)}
 */
void fabric_gather_index_nonblocking(uint64_t key, const void *local,
                                     size_t nbytes, uint64_t *index,
                                     uint64_t count, fi_addr_t fiAddr,
                                     Fam_Context *famCtx, size_t iov_limit,
                                     uint64_t base) {

    struct iovec *iov = new iovec[count];
    struct fi_rma_iov *rma_iov = new fi_rma_iov[count];

    for (uint64_t i = 0; i < count; i++) {
        iov[i].iov_base = (void *)((uint64_t)local + (i * nbytes));
        iov[i].iov_len = nbytes;

        rma_iov[i].addr = base + index[i] * nbytes;
        rma_iov[i].len = nbytes;
        rma_iov[i].key = key;
    }

    fabric_read_write_multi_msg(count, iov_limit, fiAddr, famCtx, iov, rma_iov,
                                0, 0);

    delete iov;
    delete rma_iov;

    return;
}

/*
 * fabric fence : Do with FI_FENCE write to ensure all the FAM
 * operations before the fence are completed before the other
 * FAM operations issued after fence are dispatched
 * @param famCtx - Pointer to Fam_Context
 * @param fiAddr - vector of fi_addr_t
 */
void fabric_fence(fi_addr_t fiAddr, Fam_Context *famCtx) {

    char *local = strdup("FENCE MSG");
    uint64_t nbytes = 10;
    uint64_t offset = 0;
    uint64_t key = FAM_FENCE_KEY;
    ssize_t ret;

    struct iovec iov = {.iov_base = (void *)local, .iov_len = nbytes};

    struct fi_rma_iov rma_iov = {.addr = offset, .len = nbytes, .key = key};

    struct fi_context *ctx = new struct fi_context();

    struct fi_msg_rma msg = {.msg_iov = &iov,
                             .desc = 0,
                             .iov_count = 1,
                             .addr = fiAddr,
                             .rma_iov = &rma_iov,
                             .rma_iov_count = 1,
                             .context = ctx,
                             .data = 0};

    // Take Fam_Context Write lock
    famCtx->aquire_WRLock();

    uint32_t retry_cnt = 0;

    try {
        do {
            FI_CALL(ret, fi_writemsg, famCtx->get_ep(), &msg, FI_FENCE);
        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_tx_ops();
    } catch (...) {
        // Release Fam_Context Write lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context Write lock
    famCtx->release_lock();
    delete ctx;

    return;
}

/*
 * fabric quiet : check if all non-blocking operations have completed
 *  @param famCtx - Pointer to Fam_Context
 *
 */
void fabric_put_quiet(Fam_Context *famCtx) {

    int timeout_retry_cnt = 0;

    uint64_t txsuccess = 0;
    uint64_t txfail = 0;
    uint64_t txcnt = 0;
    struct fi_cq_data_entry entry;
    ssize_t ret = 0;
    uint64_t txLastFailCnt = famCtx->get_num_tx_fail_cnt();
    int timeout_wait_retry_cnt = 0;

    txcnt = famCtx->get_num_tx_ops();

    do {
        FI_CALL(txsuccess, fi_cntr_read, famCtx->get_txCntr());
        FI_CALL(txfail, fi_cntr_readerr, famCtx->get_txCntr());

        // New failure seen; Wait for cq_read and throw exception
        if (txfail > txLastFailCnt) {
            do {
                memset(&entry, 0, sizeof(entry));
                FI_CALL(ret, fi_cq_sread, famCtx->get_txcq(), &entry, 1, NULL,
                        FABRIC_TIMEOUT);
                if (ret < 0 && (ret != -FI_EAGAIN) && (ret != -FI_ETIMEDOUT)) {
                    // Flush all the errors from completion queue
                    struct fi_cq_err_entry err;
                    FI_CALL_NO_RETURN(fi_cq_readerr, famCtx->get_txcq(), &err,
                                      0);
                    const char *errmsg =
                        fi_cq_strerror(famCtx->get_txcq(), err.prov_errno,
                                       err.err_data, NULL, 0);
                    famCtx->inc_num_tx_fail_cnt(txfail - txLastFailCnt);
                    throw Fam_Datapath_Exception(get_fam_error(err.err),
                                                 errmsg);
                }
            } while (ret < 0 &&
                     ((ret == -FI_EAGAIN) || (ret == -FI_ETIMEDOUT)));
        }

        if (timeout_retry_cnt < TIMEOUT_RETRY) {
            timeout_retry_cnt++;
        } else if (timeout_wait_retry_cnt < TIMEOUT_WAIT_RETRY) {
            timeout_wait_retry_cnt++;
            usleep(FABRIC_TIMEOUT * 1000);
        } else {
            throw Fam_Timeout_Exception("Timeout retry count exceeded INT_MAX");
        }
    } while ((txsuccess + txfail) < txcnt);

    return;
}

void fabric_get_quiet(Fam_Context *famCtx) {

    int timeout_retry_cnt = 0;

    uint64_t rxsuccess = 0;
    uint64_t rxfail = 0;
    uint64_t rxcnt = 0;
    struct fi_cq_data_entry entry;
    ssize_t ret = 0;
    uint64_t rxLastFailCnt = famCtx->get_num_rx_fail_cnt();
    int timeout_wait_retry_cnt = 0;

    rxcnt = famCtx->get_num_rx_ops();

    do {
        FI_CALL(rxsuccess, fi_cntr_read, famCtx->get_rxCntr());
        FI_CALL(rxfail, fi_cntr_readerr, famCtx->get_rxCntr());

        // New failure seen; Wait for cq_read and throw exception
        if (rxfail > rxLastFailCnt) {
            do {
                memset(&entry, 0, sizeof(entry));
                FI_CALL(ret, fi_cq_sread, famCtx->get_txcq(), &entry, 1, NULL,
                        FABRIC_TIMEOUT);
                if (ret < 0 && (ret != -FI_EAGAIN) && (ret != -FI_ETIMEDOUT)) {
                    // Flush all the errors from completion queue
                    struct fi_cq_err_entry err;
                    FI_CALL_NO_RETURN(fi_cq_readerr, famCtx->get_txcq(), &err,
                                      0);
                    const char *errmsg =
                        fi_cq_strerror(famCtx->get_txcq(), err.prov_errno,
                                       err.err_data, NULL, 0);
                    famCtx->inc_num_rx_fail_cnt(rxfail - rxLastFailCnt);
                    throw Fam_Datapath_Exception(get_fam_error(err.err),
                                                 errmsg);
                }
            } while (ret < 0 &&
                     ((ret == -FI_EAGAIN) || (ret == -FI_ETIMEDOUT)));
        }

        if (timeout_retry_cnt < TIMEOUT_RETRY) {
            timeout_retry_cnt++;
        } else if (timeout_wait_retry_cnt < TIMEOUT_WAIT_RETRY) {
            timeout_wait_retry_cnt++;
            usleep(FABRIC_TIMEOUT * 1000);
        } else {
            throw Fam_Timeout_Exception("Timeout retry count exceeded INT_MAX");
        }
    } while ((rxsuccess + rxfail) < rxcnt);

    return;
}

void fabric_quiet(Fam_Context *famCtx) {

    // Take Fam_Context Write lock
    famCtx->aquire_WRLock();

    try {
        fabric_put_quiet(famCtx);
        fabric_get_quiet(famCtx);
    } catch (...) {
        // Release Fam_Context Write lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context Write lock
    famCtx->release_lock();

    return;
}

void fabric_atomic(uint64_t key, void *value, uint64_t offset, enum fi_op op,
                   enum fi_datatype datatype, fi_addr_t fiAddr,
                   Fam_Context *famCtx) {
    struct fi_ioc iov = {.addr = value, .count = 1};

    struct fi_rma_ioc rma_iov = {.addr = offset, .count = 1, .key = key};

    struct fi_context *ctx = new struct fi_context();
    struct fi_msg_atomic msg = {.msg_iov = &iov,
                                .desc = 0,
                                .iov_count = 1,
                                .addr = fiAddr,
                                .rma_iov = &rma_iov,
                                .rma_iov_count = 1,
                                .datatype = datatype,
                                .op = op,
                                .context = ctx,
                                .data = 0};

    ssize_t ret;
    uint32_t retry_cnt = 0;

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    try {
        do {
            FI_CALL(ret, fi_atomicmsg, famCtx->get_ep(), &msg, FI_INJECT);
        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_tx_ops();
    } catch (...) {
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    return;
}

void fabric_fetch_atomic(uint64_t key, void *value, void *result,
                         uint64_t offset, enum fi_op op,
                         enum fi_datatype datatype, fi_addr_t fiAddr,
                         Fam_Context *famCtx) {
    struct fi_ioc iov = {.addr = value, .count = 1};

    struct fi_rma_ioc rma_iov = {.addr = offset, .count = 1, .key = key};

    struct fi_ioc result_iov = {.addr = result, .count = 1};

    struct fi_context *ctx = new struct fi_context();
    struct fi_msg_atomic msg = {.msg_iov = &iov,
                                .desc = 0,
                                .iov_count = 1,
                                .addr = fiAddr,
                                .rma_iov = &rma_iov,
                                .rma_iov_count = 1,
                                .datatype = datatype,
                                .op = op,
                                .context = ctx,
                                .data = 0};

    ssize_t ret;
    uint32_t retry_cnt = 0;
    uint64_t incr = 0;

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    try {
        do {
            FI_CALL(ret, fi_fetch_atomicmsg, famCtx->get_ep(), &msg,
                    &result_iov, 0, 1, FI_COMPLETION);
        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_rx_ops();
        incr++;
        ret = fabric_completion_wait(famCtx, ctx);
    } catch (...) {
        famCtx->inc_num_rx_fail_cnt(incr);
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    delete ctx;

    return;
}

void fabric_compare_atomic(uint64_t key, void *compare, void *result,
                           void *value, uint64_t offset, enum fi_op op,
                           enum fi_datatype datatype, fi_addr_t fiAddr,
                           Fam_Context *famCtx) {
    struct fi_ioc iov = {.addr = value, .count = 1};

    struct fi_rma_ioc rma_iov = {.addr = offset, .count = 1, .key = key};

    struct fi_ioc result_iov = {.addr = result, .count = 1};

    struct fi_ioc compare_iov = {.addr = compare, .count = 1};

    struct fi_context *ctx = new struct fi_context();
    struct fi_msg_atomic msg = {.msg_iov = &iov,
                                .desc = 0,
                                .iov_count = 1,
                                .addr = fiAddr,
                                .rma_iov = &rma_iov,
                                .rma_iov_count = 1,
                                .datatype = datatype,
                                .op = op,
                                .context = ctx,
                                .data = 0};

    ssize_t ret;
    uint32_t retry_cnt = 0;
    uint64_t incr = 0;

    // Take Fam_Context read lock
    famCtx->aquire_RDLock();

    try {
        do {
            FI_CALL(ret, fi_compare_atomicmsg, famCtx->get_ep(), &msg,
                    &compare_iov, 0, 1, &result_iov, 0, 1, FI_COMPLETION);

        } while (fabric_retry(famCtx, ret, &retry_cnt));
        famCtx->inc_num_rx_ops();
        incr++;
        ret = fabric_completion_wait(famCtx, ctx);
    } catch (...) {
        famCtx->inc_num_rx_fail_cnt(incr);
        // Release Fam_Context read lock
        famCtx->release_lock();
        throw;
    }

    // Release Fam_Context read lock
    famCtx->release_lock();

    delete ctx;

    return;
}
/* Fabric error string
 * @param fabErr - errno returned by libfabric fall
 * @return string
 */
const char *fabric_strerror(int fabErr) { return fi_strerror(fabErr); }

/* Fabric profile reset - resets the counters and other profile data
 */
void fabric_reset_profile() {
    LIBFABRIC_PROFILE_INIT();
    LIBFABRIC_PROFILE_START_TIME();
}

int fabric_finalize(void) {
    fabric_dump_profile();
    return 0;
}

/* Fabric profile dump - dumps the profile data
 */
void fabric_dump_profile() { LIBFABRIC_PROFILE_END(); }

/* Fabric error to fam_error
 * @param fabErr - errno returned by libfabric fall
 * @return enum Fam_Error
 */
enum Fam_Error get_fam_error(int fabErr) {
    switch (fabErr) {
    case FI_EACCES:
    case FI_EPERM:
        return FAM_ERR_NOPERM;
    default:
        return FAM_ERR_LIBFABRIC;
    }
}

} // namespace openfam

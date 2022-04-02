/*
 * fam_context.h
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
#ifndef FAM_CONTEXT_H
#define FAM_CONTEXT_H

#include <iostream>
#include <sstream>
#include <string.h>
#include <vector>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_options.h"

using namespace std;

namespace openfam {

class Fam_Context {
  public:
    Fam_Context(Fam_Thread_Model famTM);

    Fam_Context(struct fi_info *fi, struct fid_domain *domain,
                Fam_Thread_Model famTM);

    ~Fam_Context();

    struct fid_ep *get_ep() {
        return ep;
    }

    struct fid_cq *get_txcq() {
        return txcq;
    }

    struct fid_cq *get_rxcq() {
        return rxcq;
    }

    struct fid_cntr *get_txCntr() {
        return txCntr;
    }

    struct fid_cntr *get_rxCntr() {
        return rxCntr;
    }

    void inc_num_tx_ops() {
        uint64_t one = 1;
        __sync_fetch_and_add(&numTxOps, one);
    }

    void inc_num_rx_ops() {
        uint64_t one = 1;
        __sync_fetch_and_add(&numRxOps, one);
    }

    uint64_t get_num_tx_ops() { return numTxOps; }

    uint64_t get_num_rx_ops() { return numRxOps; }

    int initialize_cntr(struct fid_domain *domain, struct fid_cntr **cntr);

    void aquire_RDLock() {
        if (famThreadModel == FAM_THREAD_MULTIPLE)
            pthread_rwlock_rdlock(&ctxRWLock);
    }

    void aquire_WRLock() {
        if (famThreadModel == FAM_THREAD_MULTIPLE)
            pthread_rwlock_wrlock(&ctxRWLock);
    }

    void release_lock() {
        if (famThreadModel == FAM_THREAD_MULTIPLE)
            pthread_rwlock_unlock(&ctxRWLock);
    }

    uint64_t get_num_tx_fail_cnt() { return numLastTxFailCnt; }

    uint64_t get_num_rx_fail_cnt() { return numLastRxFailCnt; }

    void inc_num_tx_fail_cnt(uint64_t cnt) {
        __sync_fetch_and_add(&numLastTxFailCnt, cnt);
    }

    void inc_num_rx_fail_cnt(uint64_t cnt) {
        __sync_fetch_and_add(&numLastRxFailCnt, cnt);
    }

  private:
    struct fid_ep *ep;
    struct fid_cq *txcq;
    struct fid_cq *rxcq;
    struct fid_cntr *txCntr;
    struct fid_cntr *rxCntr;

    uint64_t numTxOps;
    uint64_t numRxOps;
    bool isNVMM;
    uint64_t numLastTxFailCnt;
    uint64_t numLastRxFailCnt;
    Fam_Thread_Model famThreadModel;
    pthread_rwlock_t ctxRWLock;
};

} // namespace openfam
#endif

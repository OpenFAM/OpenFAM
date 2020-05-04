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

#include <string.h>
#include <vector>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include "common/fam_options.h"
enum Fam_Req_State {
    FAM_REQ_INPROGRESS = 0,
    FAM_REQ_COMPLETED = 1,
};

class Fam_Context {
  public:
    Fam_Context(Fam_Thread_Model famTM)
        : numTxOps(0), numRxOps(0), isNVMM(true) {
        numLastRxFailCnt = 0;
        numLastTxFailCnt = 0;
        // Initialize ctxRWLock
        famThreadModel = famTM;
        if (famThreadModel == FAM_THREAD_MULTIPLE)
            pthread_rwlock_init(&ctxRWLock, NULL);
    }

    Fam_Context(struct fi_info *fi, struct fid_domain *domain,
                Fam_Thread_Model famTM) {
        numTxOps = numRxOps = 0;
        isNVMM = false;
        numLastRxFailCnt = 0;
        numLastTxFailCnt = 0;

        fi->caps = FI_RMA | FI_WRITE | FI_READ | FI_ATOMIC | FI_REMOTE_WRITE |
                   FI_REMOTE_READ;
        fi->tx_attr->op_flags = FI_DELIVERY_COMPLETE;
        fi->mode = 0;
        fi->tx_attr->mode = 0;
        fi->rx_attr->mode = 0;

        // Initialize ctxRWLock
        famThreadModel = famTM;
        if (famThreadModel == FAM_THREAD_MULTIPLE)
            pthread_rwlock_init(&ctxRWLock, NULL);

        int ret = fi_endpoint(domain, fi, &ep, NULL);
        if (ret < 0) {
            // print_fierr("fi_endpoint", ret);
            // return -1;
        }

        struct fi_cq_attr cq_attr;
        memset(&cq_attr, 0, sizeof(cq_attr));
        cq_attr.format = FI_CQ_FORMAT_DATA;
        cq_attr.wait_obj = FI_WAIT_UNSPEC;
        cq_attr.wait_cond = FI_CQ_COND_NONE;

        cq_attr.size = fi->tx_attr->size;
        ret = fi_cq_open(domain, &cq_attr, &txcq, &txcq);
        if (ret < 0) {
            // print_fierr("fi_txcq_open", ret);
            // return -1;
        }
        cq_attr.size = fi->rx_attr->size;
        ret = fi_cq_open(domain, &cq_attr, &rxcq, &rxcq);
        if (ret < 0) {
            // print_fierr("fi_rxcq_open", ret);
            // return -1;
        }

        ret = fi_ep_bind(ep, &txcq->fid, FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
        if (ret < 0) {
            // print_fierr("fi_eq_txcq_bind", ret);
            // return -1;
        }

        ret = initialize_cntr(domain, &txCntr);
        if (ret < 0) {
            // print_fierr("initialize_cntr", ret);
            // return -1;
        }

        ret =
            fi_ep_bind(ep, &txCntr->fid, FI_WRITE | FI_SEND | FI_REMOTE_WRITE);
        if (ret < 0) {
            // print_fierr("fi_ep_bind", ret);
            // return -1;
        }

        ret = fi_ep_bind(ep, &rxcq->fid, FI_RECV);
        if (ret < 0) {
            // print_fierr("fi_eq_rxcq_bind", ret);
            // return -1;
        }

        ret = initialize_cntr(domain, &rxCntr);
        if (ret < 0) {
            // print_fierr("initialize_cntr", ret);
            // return -1;
        }

        ret = fi_ep_bind(ep, &rxCntr->fid, FI_READ | FI_RECV | FI_REMOTE_READ);
        if (ret < 0) {
            // print_fierr("fi_ep_bind", ret);
            // return -1;
        }
    }

    ~Fam_Context() {
        if (!isNVMM) {
            fi_close(&ep->fid);
            fi_close(&txcq->fid);
            fi_close(&rxcq->fid);
            fi_close(&txCntr->fid);
            fi_close(&rxCntr->fid);
        }
        pthread_rwlock_destroy(&ctxRWLock);
    }

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

    int initialize_cntr(struct fid_domain *domain, struct fid_cntr **cntr) {
        int ret = 0;
        struct fi_cntr_attr cntrAttr;

        memset(&cntrAttr, 0, sizeof(cntrAttr));
        cntrAttr.events = FI_CNTR_EVENTS_COMP;
        cntrAttr.wait_obj = FI_WAIT_UNSPEC;

        ret = fi_cntr_open(domain, &cntrAttr, cntr, cntr);
        if (ret < 0) {
            // print_fierr("fi_cntr_open_txcntr", ret);
            return ret;
        }

        return ret;
    }

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

#endif

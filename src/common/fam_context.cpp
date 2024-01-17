/*
 * fam_context.cpp
 * Copyright (c) 2019, 2022-2023 Hewlett Packard Enterprise Development, LP. All
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
#include <string.h>
#include <vector>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>

#include "common/fam_context.h"
#include "common/fam_libfabric.h"
#include "common/fam_options.h"

namespace openfam {

Fam_Context::Fam_Context(Fam_Thread_Model famTM)
    : numTxOps(0), numRxOps(0), isNVMM(true) {
    numLastRxFailCnt = 0;
    numLastTxFailCnt = 0;
    // Initialize ctxRWLock
    famThreadModel = famTM;
    if (famThreadModel == FAM_THREAD_MULTIPLE)
        pthread_rwlock_init(&ctxRWLock, NULL);
}

Fam_Context::Fam_Context(struct fi_info *fi, struct fid_domain *domain,
                         Fam_Thread_Model famTM) {
    std::ostringstream message;
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
        message << "Fam libfabric fi_endpoint failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    struct fi_cq_attr cq_attr;
    memset(&cq_attr, 0, sizeof(cq_attr));
    cq_attr.format = FI_CQ_FORMAT_DATA;
    cq_attr.wait_obj = FI_WAIT_UNSPEC;
    cq_attr.wait_cond = FI_CQ_COND_NONE;

    cq_attr.size = fi->tx_attr->size;
    ret = fi_cq_open(domain, &cq_attr, &txcq, &txcq);
    if (ret < 0) {
        message << "Fam libfabric fi_cq_open failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }
    cq_attr.size = fi->rx_attr->size;
    ret = fi_cq_open(domain, &cq_attr, &rxcq, &rxcq);
    if (ret < 0) {
        message << "Fam libfabric fi_cq_open failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    ret = fi_ep_bind(ep, &txcq->fid, FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
    if (ret < 0) {
        message << "Fam libfabric fi_ep_bind failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    ret = initialize_cntr(domain, &txCntr);
    if (ret < 0) {
        // print_fierr("initialize_cntr", ret);
        // return -1;
        // throws exception in initialize_cntr
    }

    ret = fi_ep_bind(ep, &txCntr->fid, FI_WRITE | FI_SEND | FI_REMOTE_WRITE);
    if (ret < 0) {
        message << "Fam libfabric fi_ep_bind failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    ret = fi_ep_bind(ep, &rxcq->fid, FI_RECV);
    if (ret < 0) {
        message << "Fam libfabric fi_ep_bind failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    ret = initialize_cntr(domain, &rxCntr);
    if (ret < 0) {
        // print_fierr("initialize_cntr", ret);
        // return -1;
        // throws exception in initialize_cntr
    }

    ret = fi_ep_bind(ep, &rxCntr->fid, FI_READ | FI_RECV | FI_REMOTE_READ);
    if (ret < 0) {
        message << "Fam libfabric fi_ep_bind failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }
}

Fam_Context::~Fam_Context() {
    if (!isNVMM) {
        free(mr_descs);
        if (mr != NULL)
            fi_close(&mr->fid);
        fi_close(&ep->fid);
        fi_close(&txcq->fid);
        fi_close(&rxcq->fid);
        fi_close(&txCntr->fid);
        fi_close(&rxCntr->fid);
    }
    pthread_rwlock_destroy(&ctxRWLock);
}

int Fam_Context::initialize_cntr(struct fid_domain *domain,
                                 struct fid_cntr **cntr) {
    int ret = 0;
    struct fi_cntr_attr cntrAttr;
    std::ostringstream message;

    memset(&cntrAttr, 0, sizeof(cntrAttr));
    cntrAttr.events = FI_CNTR_EVENTS_COMP;
    cntrAttr.wait_obj = FI_WAIT_UNSPEC;

    ret = fi_cntr_open(domain, &cntrAttr, cntr, cntr);
    if (ret < 0) {
        message << "Fam libfabric fi_cntr_open failed: "
                << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }

    return ret;
}

void Fam_Context::register_heap(void *base, size_t len,
                                struct fid_domain *domain, size_t iov_limit) {
    std::ostringstream message;
    int ret;
    local_buf_base = base;
    local_buf_size = len;

    if (mr || mr_descs) {
        message << "Fam_Context register_heap() called more than once";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }
    mr_descs = (void **)calloc(iov_limit, sizeof(*mr_descs));
    if (!mr_descs) {
        message << "Fam_Context register_heap() failed to allocate memory";
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }
    ret = fi_mr_reg(domain, base, len, FI_READ | FI_WRITE, 0, 0, 0, &mr, 0);
    if (ret < 0) {
        message << "Fam libfabric fi_mr_reg failed: " << fabric_strerror(ret);
        THROW_ERR_MSG(Fam_Datapath_Exception, message.str().c_str());
    }
    for (size_t i = 0; i < iov_limit; i++)
        mr_descs[i] = fi_mr_desc(mr);
}

} // namespace openfam

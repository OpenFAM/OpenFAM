/*
 * fam_libfabric.h
 * Copyright (c) 2019-2021 Hewlett Packard Enterprise Development, LP. All
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
#ifndef FAM_LIBFABRIC_H
#define FAM_LIBFABRIC_H

#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <rdma/fabric.h>
#include <rdma/fi_atomic.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_eq.h>
#include <rdma/fi_rma.h>

#include "cis/fam_cis_rpc.grpc.pb.h"
#include "common/fam_context.h"
#include "common/fam_internal.h"
#include "common/fam_options.h"
#include "fam/fam_exception.h"

namespace openfam {
int fabric_initialize(const char *name, const char *service, bool source,
                      char *provider, struct fi_info **fi,
                      struct fid_fabric **fabric, struct fid_eq **eq,
                      struct fid_domain **domain, Fam_Thread_Model famTM);

void fabric_reset_profile(void);

void fabric_dump_profile(void);

int fabric_finalize(void);

int fabric_initialize_av(struct fi_info *fi, struct fid_domain *domain,
                         struct fid_eq *eq, struct fid_av **av);

int fabric_insert_av(const char *addr, struct fid_av *av,
                     std::vector<fi_addr_t> *fiAddrs);

int fabric_enable_bind_ep(struct fi_info *fi, struct fid_av *av,
                          struct fid_eq *eq, struct fid_ep *ep);

int fabric_register_mr(void *addr, size_t size, uint64_t *key,
                       struct fid_domain *domain, bool rw, fid_mr *&mr);

int fabric_deregister_mr(fid_mr *&mr);

int fabric_write(uint64_t key, const void *local, size_t nbytes,
                 uint64_t offset, fi_addr_t fiAddr, Fam_Context *famCtx);

int fabric_read(uint64_t key, const void *local, size_t nbytes, uint64_t offset,
                fi_addr_t fiAddr, Fam_Context *famCtx);

int fabric_scatter_stride_blocking(uint64_t key, const void *local,
                                   size_t nbytes, uint64_t first,
                                   uint64_t count, uint64_t stride,
                                   fi_addr_t fiAddr, Fam_Context *famCtx,
                                   size_t iov_limit, uint64_t base);

int fabric_gather_stride_blocking(uint64_t key, const void *local,
                                  size_t nbytes, uint64_t first, uint64_t count,
                                  uint64_t stride, fi_addr_t fiAddr,
                                  Fam_Context *famCtx, size_t iov_limit,
                                  uint64_t base);

int fabric_scatter_index_blocking(uint64_t key, const void *local,
                                  size_t nbytes, uint64_t *index,
                                  uint64_t count, fi_addr_t fiAddr,
                                  Fam_Context *famCtx, size_t iov_limit,
                                  uint64_t base);

int fabric_gather_index_blocking(uint64_t key, const void *local, size_t nbytes,
                                 uint64_t *index, uint64_t count,
                                 fi_addr_t fiAddr, Fam_Context *famCtx,
                                 size_t iov_limit, uint64_t base);
void fabric_write_nonblocking(uint64_t key, const void *local, size_t nbytes,
                              uint64_t offset, fi_addr_t fiAddr,
                              Fam_Context *famCtx);

void fabric_read_nonblocking(uint64_t key, const void *local, size_t nbytes,
                             uint64_t offset, fi_addr_t fiAddr,
                             Fam_Context *famCtx);

void fabric_scatter_stride_nonblocking(uint64_t key, const void *local,
                                       size_t nbytes, uint64_t first,
                                       uint64_t count, uint64_t stride,
                                       fi_addr_t fiAddr, Fam_Context *famCtx,
                                       size_t iov_limit, uint64_t base);

void fabric_gather_stride_nonblocking(uint64_t key, const void *local,
                                      size_t nbytes, uint64_t first,
                                      uint64_t count, uint64_t stride,
                                      fi_addr_t fiAddr, Fam_Context *famCtx,
                                      size_t iov_limit, uint64_t base);

void fabric_scatter_index_nonblocking(uint64_t key, const void *local,
                                      size_t nbytes, uint64_t *index,
                                      uint64_t count, fi_addr_t fiAddr,
                                      Fam_Context *famCtx, size_t iov_limit,
                                      uint64_t base);

void fabric_gather_index_nonblocking(uint64_t key, const void *local,
                                     size_t nbytes, uint64_t *index,
                                     uint64_t count, fi_addr_t fiAddr,
                                     Fam_Context *famCtx, size_t iov_limit,
                                     uint64_t base);

void fabric_fence(fi_addr_t fiAddr, Fam_Context *context);

void fabric_quiet(Fam_Context *context);

uint64_t fabric_progress(Fam_Context *context);

int fabric_retry(Fam_Context *context, int ret, uint64_t *retry_cnt);

int fabric_completion_wait(Fam_Context *famCtx, fi_context *ctx, int ioType);

void fabric_atomic(uint64_t key, void *value, uint64_t offset, enum fi_op op,
                   enum fi_datatype datatype, fi_addr_t fiAddr,
                   Fam_Context *famCtx);

void fabric_fetch_atomic(uint64_t key, void *value, void *result,
                         uint64_t offset, enum fi_op op,
                         enum fi_datatype datatype, fi_addr_t fiAddr,
                         Fam_Context *famCtx);

void fabric_compare_atomic(uint64_t key, void *value, void *result,
                           void *compare, uint64_t offset, enum fi_op op,
                           enum fi_datatype datatype, fi_addr_t fiAddr,
                           Fam_Context *famCtx);

const char *fabric_strerror(int fabErr);

int fabric_getname_len(struct fid_ep *ep, size_t *addrSize);

int fabric_getname(struct fid_ep *ep, void *addr, size_t *addrSize);

enum Fam_Error get_fam_error(int fabErr);

void fabric_send_response(void *retStatus, fi_addr_t fiAddr,
                          Fam_Context *famCtx, size_t nbytes);

fi_context *fabric_post_response_buff(void *retStatus, fi_addr_t fiAddr,
                                      Fam_Context *famCtx, size_t nbytes);

} // namespace openfam
#endif

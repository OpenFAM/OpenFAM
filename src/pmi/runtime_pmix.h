/*
 * runtime_pmix.h
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

#include "fam_runtime.h"
#include <pmix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class Pmix_Runtime : public Fam_Runtime {

  protected:
    pmix_proc_t mProc;
    pmix_status_t mInitrc;
    pmix_rank_t mRank;
    uint32_t mNumPEs;

  public:
    /*
     * Adding a dummy constructor
     */
    Pmix_Runtime() {}

    /*
     * Initialize the PMIx client library
     * Also initializes the class fields mRank and mNumPEs.
     */
    int runtime_init(void) {
        pmix_value_t *val;
        pmix_status_t rc;
        pmix_proc_t proc;
        if (PMIX_SUCCESS != (rc = PMIx_Init(&mProc, NULL, 0))) {
            mInitrc = rc;
            return rc;
        }
        mInitrc = rc;
        (void)strncpy(proc.nspace, mProc.nspace, PMIX_MAX_NSLEN);
        proc.rank = PMIX_RANK_WILDCARD;

        if (PMIX_SUCCESS ==
            (rc = PMIx_Get(&proc, PMIX_JOB_SIZE, NULL, 0, &val))) {
            mNumPEs = val->data.uint32;
            PMIX_VALUE_RELEASE(val);
        } else {
            return rc;
        }
        mRank = mProc.rank;
        return mInitrc;
    }
    /*
     * Gives the number of nodes/PEs in this job
     * @return - On successful init, returns the number of nodes associated with
     *this job.
     **/
    int num_pes(void) {
        if (PMIX_SUCCESS == mInitrc)
            return mNumPEs;
        else
            return mInitrc;
    }
    /*
     * Gives the local rank on this node within this job.
     * @return - Returns rank of current node, if init was successful.
     **/
    int my_pe(void) {

        if (PMIX_SUCCESS == mInitrc)
            return mRank;
        else
            return mInitrc;
    }

    /*
     * Finalize the PMIx client library.
     **/
    int runtime_fini() {
        pmix_status_t rc;

        rc = PMIx_Finalize(NULL, 0);
        return rc;
    }

    /*
     * Aborts all the processes that belong to client's namespace
     * @param exitCode - integer containing the exit code
     * @param msg - string that contains message to be printed before abort call
     *is initiated
     **/
    int runtime_abort(int exitCode, const char msg[]) {
        pmix_status_t rc;

        rc = PMIx_Abort(exitCode, "Aborting", NULL, 0);
        return rc;
    }

    /*
     * Suspends the execution of the calling PE until all other PEs issue
     * a call to this particular runtime_barrier_all() statement
     **/
    int runtime_barrier_all() {
        pmix_status_t rc;
        pmix_proc_t proc;

        /* call fence to sync */
        PMIX_PROC_CONSTRUCT(&proc);
        (void)strncpy(proc.nspace, mProc.nspace, PMIX_MAX_NSLEN);
        proc.rank = PMIX_RANK_WILDCARD;

        if (PMIX_SUCCESS != (rc = PMIx_Fence(&proc, 1, NULL, 0))) {
            return -1;
        }
        return 0;
    }

    /*
     * Adding a dummy destructor
     */
    ~Pmix_Runtime() {}
};

/*
 * runtime_pmi2.h
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
#include <iostream>
#include <pmi2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

class Pmi2_Runtime : public Fam_Runtime {

  protected:
    int mProc;
    int mInitrc;
    int mRank = -1;
    int mNumPEs = 0;

  public:
    /*
     * Adding a dummy constructor
     */
    Pmi2_Runtime() {}

    /*
     * Initialize the PMIx client library
     * Also initializes the class fields mRank and mNumPEs.
     */
    int runtime_init(void) {
        int rc;
        int spawned, appnum;

        if (PMI2_SUCCESS !=
            (rc = PMI2_Init(&spawned, &mNumPEs, &mRank, &appnum))) {
            mInitrc = rc;
            return rc;
        }
        mInitrc = rc;
        return mInitrc;
    }
    /*
     * Gives the number of nodes/PEs in this job
     * @return - On successful init, returns the number of nodes associated with
     *this job.
     **/
    int num_pes(void) {
        if (PMI2_SUCCESS == mInitrc)
            return mNumPEs;
        else
            return mInitrc;
    }
    /*
     * Gives the local rank on this node within this job.
     * @return - Returns rank of current node, if init was successful.
     **/
    int my_pe(void) {

        if (PMI2_SUCCESS == mInitrc)
            return mRank;
        else
            return mInitrc;
    }

    /*
     * Finalize the PMIx client library.
     **/
    int runtime_fini() {
        int rc;

        rc = PMI2_Finalize();
        return rc;
    }

    /*
     * Aborts all the processes that belong to client's namespace
     * @param exitCode - integer containing the exit code
     * @param msg - string that contains message to be printed before abort call
     *is initiated
     **/
    int runtime_abort(int exitCode, const char msg[]) {
        int rc;

        rc = PMI2_Abort(exitCode, "Aborting");
        return rc;
    }

    /*
     * Suspends the execution of the calling PE until all other PEs issue
     * a call to this particular runtime_barrier_all() statement
     **/
    int runtime_barrier_all() {
	int rc = 0;
	rc = PMI2_KVS_Fence();
        return rc;
    }

    /*
     * Adding a dummy destructor
     */
    ~Pmi2_Runtime() {}
};

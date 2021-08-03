/*
 * fam_ops.h
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
#ifndef FAM_OPS_H
#define FAM_OPS_H

#include "fam/fam.h"
#include "fam/fam_exception.h"

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "common/fam_options.h"

#include <iostream>
#include <sstream>

using namespace std;
using namespace openfam;

#define int__ true
#define void__
#define ptr__ (void *)NULL
#define FAM_OPS_UNIMPLEMENTED(type)                                            \
    {                                                                          \
        std::ostringstream message;                                            \
        message                                                                \
            << __func__                                                        \
            << " is Not Yet Implemented or fam object Not Initialized !!!";    \
        throw Fam_Unimplemented_Exception(message.str().c_str());              \
        return type;                                                           \
    }

class Fam_Ops {
  public:
    /**
     * Initialize the FAM Ops. This method is required to be the first
     * method called when a process uses the OpenFAM library.
     * @return - {true(0), false(1), errNo(<0)}
     */
    virtual int initialize() = 0;

    /**
     * Finalize the libface library. Once finalized, the process can continue
     * work, but it is disconnected from the OpenFAM library functions.
     */
    virtual void finalize() = 0;

    /**
     * Forcibly terminate all PEs in the same group as the caller
     * @param status - termination status to be returned by the program.
     */
    virtual void abort(int status) = 0;

    /**
     * Copy data from FAM to node local memory, blocking the caller while the
     * copy is completed.
     * @param descriptor - valid descriptor to area in FAM.
     * @param local - pointer to local memory region where data needs to be
     * copied. Must be of appropriate size
     * @param offset - byte offset within the space defined by the descriptor
     * from where memory should be copied
     * @param nbytes - number of bytes to be copied from global to local memory
     * @return - 0 for successful completion, 1 for unsuccessful, and a negative
     * number in case of exceptions
     */
    virtual int get_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes) = 0;

    /**
     * Initiate a copy of data from FAM to node local memory. Do not wait until
     * copy is finished
     * @param descriptor - valid descriptor to area in FAM.
     * @param local - pointer to local memory region where data needs to be
     * copied. Must be of appropriate size
     * @param offset - byte offset within the space defined by the descriptor
     * from where memory should be copied
     * @param nbytes - number of bytes to be copied from global to local memory
     */
    virtual void get_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t offset, uint64_t nbytes) = 0;

    /**
     * Copy data from local memory to FAM, blocking until the copy is complete.
     * @param local - pointer to local memory. Must point to valid data in local
     * memory
     * @param descriptor - valid descriptor in FAM
     * @param offset - byte offset within the region defined by the descriptor
     * to where data should be copied
     * @param nbytes - number of bytes to be copied from local to FAM
     * @return - 0 for successful completion, 1 for unsuccessful completion,
     * negative number in case of exceptions
     */
    virtual int put_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes) = 0;

    /**
     * Initiate a copy of data from local memory to FAM, returning before copy
     * is complete
     * @param local - pointer to local memory. Must point to valid data in local
     * memory
     * @param descriptor - valid descriptor in FAM
     * @param offset - byte offset within the region defined by the descriptor
     * to where data should be copied
     * @param nbytes - number of bytes to be copied from local to FAM
     */
    virtual void put_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t offset, uint64_t nbytes) = 0;

    // GATHER/SCATTER subgroup

    /**
     * Gather data from FAM to local memory, blocking while the copy is complete
     * Gathers disjoint elements within a data item in FAM to a contiguous array
     * in local memory. Currently constrained to gather data from a single FAM
     * descriptor, but can be extended if data needs to be gathered from
     * multiple data items.
     * @param local - pointer to local memory array. Must be large enough to
     * contain returned data
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be gathered in local memory
     * @param firstElement - first element in FAM to include in the strided
     * access
     * @param stride - stride in elements
     * @param elementSize - size of the element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case of exception
     * @see #fam_scatter_strided
     */
    virtual int gather_blocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t firstElement,
                                uint64_t stride, uint64_t elementSize) = 0;

    /**
     * Gather data from FAM to local memory, blocking while copy is complete
     * Gathers disjoint elements within a data item in FAM to a contiguous array
     * in local memory. Currently constrained to gather data from a single FAM
     * descriptor, but can be extended if data needs to be gathered from
     * multiple data items.
     * @param local - pointer to local memory array. Must be large enough to
     * contain returned data
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be gathered in local memory
     * @param elementIndex - array of element indexes in FAM to fetch
     * @param elementSize - size of each element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case errors
     * @see #fam_scatter_indexed
     */
    virtual int gather_blocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t *elementIndex,
                                uint64_t elementSize) = 0;

    /**
     * Initiate a gather of data from FAM to local memory, return before
     * completion Gathers disjoint elements within a data item in FAM to a
     * contiguous array in local memory. Currently constrained to gather data
     * from a single FAM descriptor, but can be extended if data needs to be
     * gathered from multiple data items.
     * @param local - pointer to local memory array. Must be large enough to
     * contain returned data
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be gathered in local memory
     * @param firstElement - first element in FAM to include in the strided
     * access
     * @param stride - stride in elements
     * @param elementSize - size of the element in bytes
     * @see #fam_scatter_strided
     */
    virtual void gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t nElements, uint64_t firstElement,
                                    uint64_t stride, uint64_t elementSize) = 0;

    /**
     * Gather data from FAM to local memory, blocking while copy is complete
     * Gathers disjoint elements within a data item in FAM to a contiguous array
     * in local memory. Currently constrained to gather data from a single FAM
     * descriptor, but can be extended if data needs to be gathered from
     * multiple data items.
     * @param local - pointer to local memory array. Must be large enough to
     * contain returned data
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be gathered in local memory
     * @param elementIndex - array of element indexes in FAM to fetch
     * @param elementSize - size of each element in bytes
     * @see #fam_scatter_indexed
     */
    virtual void gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                    uint64_t nElements, uint64_t *elementIndex,
                                    uint64_t elementSize) = 0;

    /**
     * Scatter data from local memory to FAM.
     * Scatters data from a contiguous array in local memory to disjoint
     * elements of a data item in FAM. Currently constrained to scatter data to
     * a single FAM descriptor, but can be extended if data needs to be
     * scattered to multiple data items.
     * @param local - pointer to local memory region containing elements
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be scattered from local memory
     * @param firstElement - placement of the first element in FAM to place for
     * the strided access
     * @param stride - stride in elements
     * @param elementSize - size of each element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case errors
     * @see #fam_gather_strided
     */
    virtual int scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t firstElement,
                                 uint64_t stride, uint64_t elementSize) = 0;

    /**
     * Scatter data from local memory to FAM.
     * Scatters data from a contiguous array in local memory to disjoint
     * elements of a data item in FAM. Currently constrained to scatter data to
     * a single FAM descriptor, but can be extended if data needs to be
     * scattered to multiple data items.
     * @param local - pointer to local memory region containing data elements
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be scattered from local memory
     * @param elementIndex - array containing element indexes
     * @param elementSize - size of the element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case errors
     * @see #fam_gather_indexed
     */
    virtual int scatter_blocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t *elementIndex,
                                 uint64_t elementSize) = 0;

    /**
     * initiate a scatter data from local memory to FAM.
     * Scatters data from a contiguous array in local memory to disjoint
     * elements of a data item in FAM. Currently constrained to scatter data to
     * a single FAM descriptor, but can be extended if data needs to be
     * scattered to multiple data items.
     * @param local - pointer to local memory region containing elements
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be scattered from local memory
     * @param firstElement - placement of the first element in FAM to place for
     * the strided access
     * @param stride - stride in elements
     * @param elementSize - size of each element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case errors
     * @see #fam_gather_strided
     */
    virtual void scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t firstElement,
                                     uint64_t stride, uint64_t elementSize) = 0;

    /**
     * Initiate a scatter data from local memory to FAM.
     * Scatters data from a contiguous array in local memory to disjoint
     * elements of a data item in FAM. Currently constrained to scatter data to
     * a single FAM descriptor, but can be extended if data needs to be
     * scattered to multiple data items.
     * @param local - pointer to local memory region containing data elements
     * @param descriptor - valid descriptor containing FAM reference
     * @param nElements - number of elements to be scattered from local memory
     * @param elementIndex - array containing element indexes
     * @param elementSize - size of the element in bytes
     * @return - 0 for normal completion, 1 in case of unsuccessful completion,
     * negative number in case errors
     * @see #fam_gather_indexed
     */
    virtual void scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                     uint64_t nElements, uint64_t *elementIndex,
                                     uint64_t elementSize) = 0;

    // COPY Subgroup

    /**
     * Copy data from one FAM-resident data item to another FAM-resident data
     * item (potentially within a different region).
     * @param src - valid descriptor to source data item in FAM.
     * @param srcOffset - byte offset within the space defined by the src
     * descriptor from which memory should be copied.
     * @param dest - valid descriptor to destination data item in FAM.
     * @param destOffset - byte offset within the space defined by the dest
     * descriptor to which memory should be copied.
     * @param nbytes - number of bytes to be copied
     */
    virtual void *copy(Fam_Descriptor *src, uint64_t srcOffset,
                       Fam_Descriptor *dest, uint64_t destOffset,
                       uint64_t nbytes) = 0;

    virtual void wait_for_copy(void *waitObj) = 0;
    virtual void backup(Fam_Descriptor *desc, char *Backup_Name) = 0;
    virtual void restore(char *inputFile, Fam_Descriptor *dest) = 0;

    // ATOMICS Group

    // NON fetching routines

    /**
     * set group - atomically set a value at a given offset within a data item
     * in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be set at the given location
     */
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            int32_t value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            int64_t value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            int128_t value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            float value) = 0;
    virtual void atomic_set(Fam_Descriptor *descriptor, uint64_t offset,
                            double value) = 0;

    /**
     * add group - atomically add a value to the value at a given offset within
     * a data item in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be added to the existing value at the given
     * location
     */
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            int32_t value) = 0;
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            int64_t value) = 0;
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            float value) = 0;
    virtual void atomic_add(Fam_Descriptor *descriptor, uint64_t offset,
                            double value) = 0;

    /**
     * subtract group - atomically subtract a value from a value at a given
     * offset within a data item in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be subtracted from the existing value at the
     * given location
     */
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 int32_t value) = 0;
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 int64_t value) = 0;
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint32_t value) = 0;
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 uint64_t value) = 0;
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 float value) = 0;
    virtual void atomic_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                 double value) = 0;

    /**
     * min group - atomically set the value at a given offset within a data item
     * in FAM to the smaller of the existing value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared to the existing value at the given
     * location
     */
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            int32_t value) = 0;
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            int64_t value) = 0;
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            float value) = 0;
    virtual void atomic_min(Fam_Descriptor *descriptor, uint64_t offset,
                            double value) = 0;

    /**
     * max group - atomically set the value at a given offset within a data item
     * in FAM to the larger of the value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared to the existing value at the given
     * location
     */
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            int32_t value) = 0;
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            int64_t value) = 0;
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            float value) = 0;
    virtual void atomic_max(Fam_Descriptor *descriptor, uint64_t offset,
                            double value) = 0;

    /**
     * and group - atomically replace the value at a given offset within a data
     * item in FAM with the logical AND of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     */
    virtual void atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_and(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;

    /**
     * or group - atomically replace the value at a given offset within a data
     * item in FAM with the logical OR of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     */
    virtual void atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value) = 0;
    virtual void atomic_or(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value) = 0;

    /**
     * xor group - atomically replace the value at a given offset within a data
     * item in FAM with the logical XOR of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     */
    virtual void atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                            uint32_t value) = 0;
    virtual void atomic_xor(Fam_Descriptor *descriptor, uint64_t offset,
                            uint64_t value) = 0;

    // FETCHING Routines - perform the operation, and return the old value in
    // FAM

    /**
     * fetch group - atomically fetches the value at the given offset within a
     * data item from FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @return - value from the given location in FAM
     */
    virtual int32_t atomic_fetch_int32(Fam_Descriptor *descriptor,
                                       uint64_t offset) = 0;
    virtual int64_t atomic_fetch_int64(Fam_Descriptor *descriptor,
                                       uint64_t offset) = 0;
    virtual int128_t atomic_fetch_int128(Fam_Descriptor *descriptor,
                                         uint64_t offset) = 0;
    virtual uint32_t atomic_fetch_uint32(Fam_Descriptor *descriptor,
                                         uint64_t offset) = 0;
    virtual uint64_t atomic_fetch_uint64(Fam_Descriptor *descriptor,
                                         uint64_t offset) = 0;
    virtual float atomic_fetch_float(Fam_Descriptor *descriptor,
                                     uint64_t offset) = 0;
    virtual double atomic_fetch_double(Fam_Descriptor *descriptor,
                                       uint64_t offset) = 0;

    /**
     * swap group - atomically replace the value at the given offset within a
     * data item in FAM with the given value, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be swapped with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual int32_t swap(Fam_Descriptor *descriptor, uint64_t offset,
                         int32_t value) = 0;
    virtual int64_t swap(Fam_Descriptor *descriptor, uint64_t offset,
                         int64_t value) = 0;
    virtual uint32_t swap(Fam_Descriptor *descriptor, uint64_t offset,
                          uint32_t value) = 0;
    virtual uint64_t swap(Fam_Descriptor *descriptor, uint64_t offset,
                          uint64_t value) = 0;
    virtual float swap(Fam_Descriptor *descriptor, uint64_t offset,
                       float value) = 0;
    virtual double swap(Fam_Descriptor *descriptor, uint64_t offset,
                        double value) = 0;

    /**
     * compare and swap group - atomically conditionally replace the value at
     * the given offset within a data item in FAM with the given value, and
     * return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param oldValue - value to be compared with the existing value at the
     * given location
     * @param newValue - new value to be stored if comparison is successful
     * @return - old value from the given location in FAM
     */
    virtual int32_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 int32_t oldValue, int32_t newValue) = 0;
    virtual int64_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                 int64_t oldValue, int64_t newValue) = 0;
    virtual uint32_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint32_t oldValue, uint32_t newValue) = 0;
    virtual uint64_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                  uint64_t oldValue, uint64_t newValue) = 0;
    virtual int128_t compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                                  int128_t oldValue, int128_t newValue) = 0;

    /**
     * fetch and add group - atomically add the given value to the value at the
     * given offset within a data item in FAM, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be added to the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual int32_t atomic_fetch_add(Fam_Descriptor *descriptor,
                                     uint64_t offset, int32_t value) = 0;
    virtual int64_t atomic_fetch_add(Fam_Descriptor *descriptor,
                                     uint64_t offset, int64_t value) = 0;
    virtual uint32_t atomic_fetch_add(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_add(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) = 0;
    virtual float atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) = 0;
    virtual double atomic_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                                    double value) = 0;

    /**
     * fetch and subtract group - atomically subtract the given value from the
     * value at the given offset within a data item in FAM, and return the old
     * value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be subtracted from the existing value at the
     * given location
     * @return - old value from the given location in FAM
     */
    virtual int32_t atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                          uint64_t offset, int32_t value) = 0;
    virtual int64_t atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                          uint64_t offset, int64_t value) = 0;
    virtual uint32_t atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                           uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                           uint64_t offset, uint64_t value) = 0;
    virtual float atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                        uint64_t offset, float value) = 0;
    virtual double atomic_fetch_subtract(Fam_Descriptor *descriptor,
                                         uint64_t offset, double value) = 0;

    /**
     * fetch and min group - atomically set the value at a given offset within a
     * data item in FAM to the smaller of the existing value and the given
     * value, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual int32_t atomic_fetch_min(Fam_Descriptor *descriptor,
                                     uint64_t offset, int32_t value) = 0;
    virtual int64_t atomic_fetch_min(Fam_Descriptor *descriptor,
                                     uint64_t offset, int64_t value) = 0;
    virtual uint32_t atomic_fetch_min(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_min(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) = 0;
    virtual float atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) = 0;
    virtual double atomic_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                                    double value) = 0;

    /**
     * fetch and max group - atomically set the value at a given offset within a
     * data item in FAM to the larger of the existing value and the given value,
     * and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual int32_t atomic_fetch_max(Fam_Descriptor *descriptor,
                                     uint64_t offset, int32_t value) = 0;
    virtual int64_t atomic_fetch_max(Fam_Descriptor *descriptor,
                                     uint64_t offset, int64_t value) = 0;
    virtual uint32_t atomic_fetch_max(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_max(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) = 0;
    virtual float atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                   float value) = 0;
    virtual double atomic_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                                    double value) = 0;

    /**
     * fetch and and group - atomically replace the value at a given offset
     * within a data item in FAM with the logical AND of that value and the
     * given value, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual uint32_t atomic_fetch_and(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_and(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) = 0;

    /**
     * fetch and or group - atomically replace the value at a given offset
     * within a data item in FAM with the logical OR of that value and the given
     * value, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual uint32_t atomic_fetch_or(Fam_Descriptor *descriptor,
                                     uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_or(Fam_Descriptor *descriptor,
                                     uint64_t offset, uint64_t value) = 0;

    /**
     * fetch and xor group - atomically replace the value at a given offset
     * within a data item in FAM with the logical XOR of that value and the
     * given value, and return the old value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - old value from the given location in FAM
     */
    virtual uint32_t atomic_fetch_xor(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint32_t value) = 0;
    virtual uint64_t atomic_fetch_xor(Fam_Descriptor *descriptor,
                                      uint64_t offset, uint64_t value) = 0;

    // MEMORY ORDERING Routines - provide ordering of FAM operations issued by a
    // PE

    /**
     * fam_fence - ensures that FAM operations (put, scatter, atomics, copy)
     * issued by the calling PE thread before the fence are ordered before FAM
     * operations issued by the calling thread after the fence Note that method
     * this does NOT order load/store accesses by the processor to FAM enabled
     * by fam_map().
     */
    virtual void fence(Fam_Region_Descriptor *descriptor = NULL) = 0;

    /**
     * fam_quiet - blocks the calling PE thread until all its pending FAM
     * operations (put, scatter, atomics, copy) are completed. Note that method
     * this does NOT order or wait for completion of load/store accesses by the
     * processor to FAM enabled by fam_map().
     */
    virtual void quiet(Fam_Region_Descriptor *descriptor = NULL) = 0;

    /**
     * progress - returns number of all its pending FAM
     * operations (put, scatter, atomics, copy).
     * @return - number of pending operations
     */
    virtual uint64_t progress() = 0;

    /**
     * check_progress thread is used by memory server to keep the I/Os going on.
     */
    virtual void check_progress(Fam_Region_Descriptor *descriptor = NULL) = 0;
    /**
     * fam() - constructor for fam class
     */
    Fam_Ops(){};

    /**
     * ~fam() - destructor for fam class
     */
    virtual ~Fam_Ops(){};
};

#endif

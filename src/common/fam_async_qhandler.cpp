/*
 * fam_async_qhandler.cpp
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

#include <boost/lockfree/queue.hpp>
#include <boost/thread/thread.hpp>

#include <boost/atomic.hpp>
#include <boost/fiber/condition_variable.hpp>

#include <iostream>

#include "common/fam_async_qhandler.h"
namespace openfam {

class Fam_Async_QHandler::FamAsyncQHandlerImpl_ {
  public:
    FamAsyncQHandlerImpl_(uint64_t numConsumer) {
        readCtr = 0;
        writeCtr = 0;
        readErrCtr = 0;
        writeErrCtr = 0;
        run = true;
        queue = new boost::lockfree::queue<Fam_Ops_Info>(1024);
        readCQ = new boost::lockfree::queue<Fam_Async_Err *>(1024);
        writeCQ = new boost::lockfree::queue<Fam_Async_Err *>(1024);
        for (uint64_t i = 0; i < numConsumer; i++) {
            consumerThreads.create_thread(boost::bind(
                &FamAsyncQHandlerImpl_::nonblocking_ops_handler, this));
        }
    }

    ~FamAsyncQHandlerImpl_() {
        run = false;
        consumerThreads.join_all();
        delete queue;
    }

    void nonblocking_ops_handler(void) {
        Fam_Ops_Info opsInfo;

        while (run) {
            if (queue->pop(opsInfo))
                decode_and_execute(opsInfo);
        }
    }

    void initiate_operation(Fam_Ops_Info opsInfo) {
        queue->push(opsInfo);
        return;
    }

    void quiet(Fam_Context *famCtx) {

        write_quiet(famCtx->get_num_tx_ops());
        read_quiet(famCtx->get_num_rx_ops());

        return;
    }

    void write_quiet(uint64_t ctr) {
        {
            std::unique_lock<boost::fibers::mutex> lk(writeMtx);
            while (!(ctr == writeCtr.load(boost::memory_order_seq_cst))) {
                writeCond.wait(lk);
            }
        }
        if (writeErrCtr.load(boost::memory_order_seq_cst) != 0) {
            writeErrCtr.store(0, boost::memory_order_seq_cst);
            Fam_Async_Err *err;
            while (writeCQ->pop(err)) {
                if (!(err->get_error_code()))
                    return;
                else
                    throw Fam_Datapath_Exception(err->get_error_code(),
                                                 err->get_error_msg());
            }
        }
    }

    void read_quiet(uint64_t ctr) {
        {
            std::unique_lock<boost::fibers::mutex> lk(readMtx);
            while (!(ctr == readCtr.load(boost::memory_order_seq_cst))) {
                readCond.wait(lk);
            }
        }
        if (readErrCtr.load(boost::memory_order_seq_cst) != 0) {
            readErrCtr.store(0, boost::memory_order_seq_cst);
            Fam_Async_Err *err;
            while (readCQ->pop(err)) {
                if (!(err->get_error_code()))
                    return;
                else
                    throw Fam_Datapath_Exception(err->get_error_code(),
                                                 err->get_error_msg());
            }
        }
    }

    void wait_for_copy(void *waitObj) {
        Copy_Tag *tag = static_cast<Copy_Tag *>(waitObj);
        {
            std::unique_lock<boost::fibers::mutex> lk(copyMtx);
            while (!tag->copyDone.load(boost::memory_order_seq_cst)) {
                copyCond.wait(lk);
            }
        }
        return;
    }

    void decode_and_execute(Fam_Ops_Info opsInfo) {
        switch (opsInfo.opsType) {
        case WRITE: {
            write_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                          opsInfo.offset, opsInfo.upperBound, opsInfo.key,
                          opsInfo.itemSize);
            break;
        }
        case READ: {
            read_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                         opsInfo.offset, opsInfo.upperBound, opsInfo.key,
                         opsInfo.itemSize);
            break;
        }
        case COPY: {
            copy_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                         opsInfo.tag);
            break;
        }
        default: {
            throw Fam_Datapath_Exception(FAM_ERR_INVALIDOP,
                                         "invalid operation request");
        }
        }
        return;
    }

    void write_handler(void *src, void *dest, uint64_t nbytes, uint64_t offset,
                       uint64_t upperBound, uint64_t key, uint64_t itemSize) {
        bool isError = false;
        Fam_Async_Err *err = new Fam_Async_Err();
        if ((offset > itemSize) || (upperBound > itemSize)) {
            err->set_error_code(FAM_ERR_OUTOFRANGE);
            err->set_error_msg("offset or data size is out of bound");
            writeErrCtr++;
            isError = true;
        } else if ((key & FAM_WRITE_KEY_SHM) != FAM_WRITE_KEY_SHM) {
            err->set_error_code(FAM_ERR_NOPERM);
            err->set_error_msg("not permitted to write into dataitem");
            writeErrCtr++;
            isError = true;
        }

        if (isError) {
            writeCQ->push(err);
        } else {
            memcpy(dest, src, nbytes);
            openfam_persist(dest, nbytes);
            delete err;
        }

        {
            std::unique_lock<boost::fibers::mutex> lk(writeMtx);
            writeCtr++;
        }
        writeCond.notify_one();
        return;
    }

    void read_handler(void *src, void *dest, uint64_t nbytes, uint64_t offset,
                      uint64_t upperBound, uint64_t key, uint64_t itemSize) {
        bool isError = false;
        Fam_Async_Err *err = new Fam_Async_Err();
        if ((offset > itemSize) || (upperBound > itemSize)) {
            err->set_error_code(FAM_ERR_OUTOFRANGE);
            err->set_error_msg("offset or data size is out of bound");
            readErrCtr++;
            isError = true;
        } else if ((key & FAM_READ_KEY_SHM) != FAM_READ_KEY_SHM) {
            err->set_error_code(FAM_ERR_NOPERM);
            err->set_error_msg("not permitted to read from dataitem");
            readErrCtr++;
            isError = true;
        }

        if (isError) {
            readCQ->push(err);
        } else {
            openfam_invalidate(src, nbytes);
            memcpy(dest, src, nbytes);
            delete err;
        }

        {
            std::unique_lock<boost::fibers::mutex> lk(readMtx);
            readCtr++;
        }
        readCond.notify_one();
        return;
    }

    void copy_handler(void *src, void *dest, uint64_t nbytes, Copy_Tag *tag) {
        memcpy(dest, src, nbytes);
        openfam_persist(dest, nbytes);

        {
            std::unique_lock<boost::fibers::mutex> lk(copyMtx);
            tag->copyDone.store(true, boost::memory_order_seq_cst);
        }
        copyCond.notify_one();
        return;
    }

  private:
    boost::lockfree::queue<Fam_Ops_Info> *queue;
    boost::lockfree::queue<Fam_Async_Err *> *readCQ, *writeCQ;
    boost::thread_group consumerThreads;
    boost::fibers::condition_variable readCond, writeCond, copyCond;
    boost::fibers::mutex readMtx, writeMtx, copyMtx;
    boost::atomic_uint64_t readCtr, writeCtr, readErrCtr, writeErrCtr;
    boost::atomic<bool> run;
};

Fam_Async_QHandler::Fam_Async_QHandler(uint64_t numConsumer) {
    fAsyncQHandler_ = new FamAsyncQHandlerImpl_(numConsumer);
}

Fam_Async_QHandler::~Fam_Async_QHandler() { delete fAsyncQHandler_; }

void Fam_Async_QHandler::nonblocking_ops_handler(void) {
    fAsyncQHandler_->nonblocking_ops_handler();
}

void Fam_Async_QHandler::initiate_operation(Fam_Ops_Info opsInfo) {
    fAsyncQHandler_->initiate_operation(opsInfo);
}

void Fam_Async_QHandler::quiet(Fam_Context *famCtx) {
    fAsyncQHandler_->quiet(famCtx);
}

void Fam_Async_QHandler::write_quiet(uint64_t ctr) {
    fAsyncQHandler_->write_quiet(ctr);
}

void Fam_Async_QHandler::read_quiet(uint64_t ctr) {
    fAsyncQHandler_->read_quiet(ctr);
}

void Fam_Async_QHandler::wait_for_copy(void *waitObj) {
    fAsyncQHandler_->wait_for_copy(waitObj);
}

void Fam_Async_QHandler::decode_and_execute(Fam_Ops_Info opsInfo) {
    fAsyncQHandler_->decode_and_execute(opsInfo);
}

void Fam_Async_QHandler::write_handler(void *src, void *dest, uint64_t nbytes,
                                       uint64_t offset, uint64_t upperBound,
                                       uint64_t key, uint64_t itemSize) {
    fAsyncQHandler_->write_handler(src, dest, nbytes, offset, upperBound, key,
                                   itemSize);
}

void Fam_Async_QHandler::read_handler(void *src, void *dest, uint64_t nbytes,
                                      uint64_t offset, uint64_t upperBound,
                                      uint64_t key, uint64_t itemSize) {
    fAsyncQHandler_->read_handler(src, dest, nbytes, offset, upperBound, key,
                                  itemSize);
}

void Fam_Async_QHandler::copy_handler(void *src, void *dest, uint64_t nbytes,
                                      Copy_Tag *tag) {
    fAsyncQHandler_->copy_handler(src, dest, nbytes, tag);
}

} // namespace openfam

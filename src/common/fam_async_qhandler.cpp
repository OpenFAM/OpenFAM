/*
 * fam_async_qhandler.cpp
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

#include <boost/lockfree/queue.hpp>
#include <boost/thread/thread.hpp>
#include <future>

#ifdef USE_BOOST_FIBER
#include <boost/fiber/condition_variable.hpp>
#else
#include <condition_variable>
#include <mutex>
#endif

#ifdef USE_BOOST_FIBER
#define AQUIRE_MUTEX(mtx) std::unique_lock<boost::fibers::mutex> lk(mtx);
#else
#define AQUIRE_MUTEX(mtx) std::unique_lock<std::mutex> lk(mtx);
#endif

#include <iostream>

#include "common/fam_async_qhandler.h"
#include "common/fam_internal_exception.h"
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
            {
                AQUIRE_MUTEX(queueMtx);
                while (queue->empty())
                    queueCond.wait(lk);
            }
        }
    }

    void initiate_operation(Fam_Ops_Info opsInfo) {
        queue->push(opsInfo);
        {
            AQUIRE_MUTEX(queueMtx);
            queueCond.notify_one();
        }
        return;
    }

    uint64_t read_progress(Fam_Context *famCtx) {
        uint64_t total_reads, ctr = 0;
        total_reads = famCtx->get_num_rx_ops();
        ctr = readCtr.load(boost::memory_order_seq_cst);
        return (total_reads - ctr);
    }
    uint64_t write_progress(Fam_Context *famCtx) {
        uint64_t ctr, writes = 0;
        writes = famCtx->get_num_tx_ops();
        ctr = writeCtr.load(boost::memory_order_seq_cst);
        return (writes - ctr);
    }

    uint64_t progress(Fam_Context *famCtx) {
        uint64_t writes, reads = 0;
        writes = write_progress(famCtx);
        reads = read_progress(famCtx);
        return (writes + reads);
    }
    void quiet(Fam_Context *famCtx) {

        qreadCtr = famCtx->get_num_rx_ops();
        qwriteCtr = famCtx->get_num_tx_ops();
        write_quiet(famCtx->get_num_tx_ops());
        read_quiet(famCtx->get_num_rx_ops());

        return;
    }

    void write_quiet(uint64_t ctr) {
        {
            AQUIRE_MUTEX(writeMtx);
            while (!(ctr == writeCtr.load(boost::memory_order_seq_cst))) {
                writeCond.wait(lk);
            }
        }
        if (writeErrCtr.load(boost::memory_order_seq_cst) != 0) {
            writeErrCtr.store(0, boost::memory_order_seq_cst);
            Fam_Async_Err *err;
            while (writeCQ->pop(err)) {
                if (!(err->get_error_code())) {
                    delete err;
                    return;
                } else {
                    Fam_Error errCode = err->get_error_code();
                    string errMsg = err->get_error_msg();
                    delete err;
                    THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode,
                                    errMsg.c_str());
                }
            }
        }
    }

    void read_quiet(uint64_t ctr) {
        {
            AQUIRE_MUTEX(readMtx);
            while (!(ctr == readCtr.load(boost::memory_order_seq_cst))) {
                readCond.wait(lk);
            }
        }
        if (readErrCtr.load(boost::memory_order_seq_cst) != 0) {
            readErrCtr.store(0, boost::memory_order_seq_cst);
            Fam_Async_Err *err;
            while (readCQ->pop(err)) {
                if (!(err->get_error_code())) {
                    delete err;
                    return;
                } else {
                    Fam_Error errCode = err->get_error_code();
                    string errMsg = err->get_error_msg();
                    delete err;
                    THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode,
                                    errMsg.c_str());
                }
            }
        }
    }

    void wait_for_copy(void *waitObj) {
        Fam_Copy_Tag *tag = static_cast<Fam_Copy_Tag *>(waitObj);
        {
            AQUIRE_MUTEX(copyMtx);
            while (!tag->copyDone.load(boost::memory_order_seq_cst)) {
                copyCond.wait(lk);
            }
        }

        if (tag->err) {
            Fam_Error errCode = tag->err->get_error_code();
            string errMsg = tag->err->get_error_msg();
            delete tag->err;
            delete tag;
            THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode, errMsg.c_str());
        }
        delete tag;
        return;
    }

    void wait_for_backup(void *waitObj) {
        Fam_Backup_Tag *tag = static_cast<Fam_Backup_Tag *>(waitObj);
        {
            AQUIRE_MUTEX(backupMtx);
            while (!tag->backupDone.load(boost::memory_order_seq_cst)) {
                backupCond.wait(lk);
            }
        }

        if (tag->err) {
            Fam_Error errCode = tag->err->get_error_code();
            string errMsg = tag->err->get_error_msg();
            delete tag->err;
            delete tag;
            THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode, errMsg.c_str());
        }

        delete tag;
        return;
    }

    void wait_for_restore(void *waitObj) {
        Fam_Restore_Tag *tag = static_cast<Fam_Restore_Tag *>(waitObj);
        {
            AQUIRE_MUTEX(restoreMtx);
            while (!tag->restoreDone.load(boost::memory_order_seq_cst)) {
                restoreCond.wait(lk);
            }
        }

        if (tag->err) {
            Fam_Error errCode = tag->err->get_error_code();
            string errMsg = tag->err->get_error_msg();
            delete tag->err;
            delete tag;
            THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode, errMsg.c_str());
        }

        delete tag;
        return;
    }

    void wait_for_delete_backup(void *waitObj) {
        Fam_Delete_Backup_Tag *tag =
            static_cast<Fam_Delete_Backup_Tag *>(waitObj);
        {
            AQUIRE_MUTEX(deletebackupMtx);
            while (!tag->delbackupDone.load(boost::memory_order_seq_cst)) {
                deletebackupCond.wait(lk);
            }
        }

        if (tag->err) {
            Fam_Error errCode = tag->err->get_error_code();
            string errMsg = tag->err->get_error_msg();
            delete tag->err;
            delete tag;
            THROW_ERRNO_MSG(Fam_Datapath_Exception, errCode, errMsg.c_str());
        }

        delete tag;
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
                         (Fam_Copy_Tag *)opsInfo.tag);
            break;
        }
	case BACKUP: {
	    backup_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                         (Fam_Backup_Tag *)opsInfo.tag);
	    break;
	}
	case RESTORE: {
	    restore_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                         (Fam_Restore_Tag *)opsInfo.tag);
	    break;
	}
        case DELETE_BACKUP: {
            delete_backup_handler(opsInfo.src, opsInfo.dest, opsInfo.nbytes,
                                  (Fam_Delete_Backup_Tag *)opsInfo.tag);
            break;
        }

        default: {
            THROW_ERRNO_MSG(Fam_Datapath_Exception, FAM_ERR_INVALIDOP,
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
            AQUIRE_MUTEX(writeMtx);
            writeCtr++;
        }
        if (qwriteCtr == writeCtr)
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
            AQUIRE_MUTEX(readMtx);
            readCtr++;
        }
        if (qreadCtr == readCtr)
            readCond.notify_one();
        return;
    }

    void copy_handler(void *src, void *dest, uint64_t nbytes,
                      Fam_Copy_Tag *tag) {
        if (tag->memoryServiceMap) {
            ostringstream message;
            uint64_t srcCopyEnd = tag->srcCopyStart + tag->size;
            uint64_t destStartServerIdx =
                (tag->destUsedMemsrvCnt == 1)
                    ? 0
                    : ((tag->destCopyStart / tag->destInterleaveSize) %
                       tag->destUsedMemsrvCnt);
            uint64_t destFamPtr =
                (tag->destUsedMemsrvCnt == 1)
                    ? tag->destCopyStart
                    : (((tag->destCopyStart / tag->destInterleaveSize) -
                        destStartServerIdx) /
                       tag->destUsedMemsrvCnt) *
                          tag->destInterleaveSize;
            uint64_t destDisplacement =
                (tag->destUsedMemsrvCnt == 1)
                    ? 0
                    : (tag->destCopyStart % tag->destInterleaveSize);
            std::list<std::shared_future<void>> resultList;
            for (int i = 0; i < (int)tag->destUsedMemsrvCnt; i++) {
                int index =
                    (i + (int)destStartServerIdx) % (int)tag->destUsedMemsrvCnt;
                auto obj =
                    tag->memoryServiceMap->find((tag->destMemserverIds)[index]);
                if (obj == tag->memoryServiceMap->end()) {
                    message << "Memory service RPC client not found";
                    THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RPC_CLIENT_NOTFOUND,
                                    message.str().c_str());
                }
                Fam_Memory_Service *memoryService = obj->second;
                uint64_t additionalOffset =
                    (index == (int)destStartServerIdx) ? destDisplacement : 0;
                std::future<void> result(std::async(
                    std::launch::async, &openfam::Fam_Memory_Service::copy,
                    memoryService, tag->srcRegionId, tag->srcOffsets,
                    tag->srcUsedMemsrvCnt, tag->srcCopyStart, srcCopyEnd,
                    tag->srcKeys, tag->srcBaseAddrList, tag->destRegionId,
                    tag->destOffsets[index] + destFamPtr + additionalOffset,
                    tag->destUsedMemsrvCnt, tag->srcMemserverIds,
                    tag->srcInterleaveSize, tag->destInterleaveSize,
                    tag->size));
                resultList.push_back(result.share());
                tag->srcCopyStart +=
                    (tag->destInterleaveSize - additionalOffset);
            }
            // Wait for region destroy to complete.
            try {
                for (auto result : resultList) {
                    result.get();
                }
            } catch (Fam_Exception &e) {
                Fam_Async_Err *err = new Fam_Async_Err();
                err->set_error_code((Fam_Error)e.fam_error());
                err->set_error_msg(e.fam_error_msg());
                tag->err = err;
            }
        } else {
            memcpy(dest, src, nbytes);
            openfam_persist(dest, nbytes);
        }
        {
            AQUIRE_MUTEX(copyMtx)
            tag->copyDone.store(true, boost::memory_order_seq_cst);
        }
        copyCond.notify_one();
        return;
    }

    void backup_handler(void *src, void *dest, uint64_t nbytes,
                      Fam_Backup_Tag *tag) {
        if (tag->memoryServiceMap) {
            ostringstream message;
            bool writeMetadata = true;
            std::list<std::shared_future<void>> resultList;
            for (int i = 0; i < (int)tag->usedMemserverCnt; i++) {
                auto obj =
                    tag->memoryServiceMap->find((tag->srcMemserverIds)[i]);
                if (obj == tag->memoryServiceMap->end()) {
                    message << "Memory service RPC client not found";
                    THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RPC_CLIENT_NOTFOUND,
                                    message.str().c_str());
                }
                Fam_Memory_Service *memoryService = obj->second;
                uint64_t size = tag->sizePerServer;
                if (tag->extraBlocks) {
                    size += tag->srcInterleaveSize;
                    tag->extraBlocks--;
                }

                std::future<void> result(std::async(
                    std::launch::async, &openfam::Fam_Memory_Service::backup,
                    memoryService, tag->srcRegionId, tag->srcOffsets[i], size,
                    tag->chunkSize, tag->usedMemserverCnt, i, tag->BackupName,
                    tag->uid, tag->gid, tag->mode, tag->dataitemName,
                    tag->srcItemSize, writeMetadata));
                resultList.push_back(result.share());
                writeMetadata = false;
            }

            // Wait for region destroy to complete.
            try {
                for (auto result : resultList) {
                    result.get();
                }
            } catch (Fam_Exception &e) {
                Fam_Async_Err *err = new Fam_Async_Err();
                err->set_error_code((Fam_Error)e.fam_error());
                err->set_error_msg(e.fam_error_msg());
                tag->err = err;
            }
        }
        {
            AQUIRE_MUTEX(backupMtx)
            tag->backupDone.store(true, boost::memory_order_seq_cst);
        }
        backupCond.notify_one();
        return;
    }

    void restore_handler(void *src, void *dest, uint64_t nbytes,
                      Fam_Restore_Tag *tag) {
        if (tag->memoryServiceMap) {
            ostringstream message;
            std::list<std::shared_future<void>> resultList;
            for (int i = 0; i < (int)tag->iterations; i++) {
                auto obj =
                    tag->memoryServiceMap->find((tag->destMemserverIds)[i]);
                if (obj == tag->memoryServiceMap->end()) {
                    message << "Memory service RPC client not found";
                    THROW_ERRNO_MSG(CIS_Exception, FAM_ERR_RPC_CLIENT_NOTFOUND,
                                    message.str().c_str());
                }
                Fam_Memory_Service *memoryService = obj->second;
                uint64_t size = tag->sizePerServer;
                if (tag->extraBlocks) {
                    size += tag->destInterleaveSize;
                    tag->extraBlocks--;
                }

                std::future<void> result(std::async(
                    std::launch::async, &openfam::Fam_Memory_Service::restore,
                    memoryService, tag->destRegionId, tag->destOffsets[i], size,
                    tag->chunkSize, tag->usedMemserverCnt, i, tag->BackupName));
                resultList.push_back(result.share());
            }
            // Wait for region destroy to complete.
            try {
                for (auto result : resultList) {
                    result.get();
                }
            } catch (Fam_Exception &e) {
                Fam_Async_Err *err = new Fam_Async_Err();
                err->set_error_code((Fam_Error)e.fam_error());
                err->set_error_msg(e.fam_error_msg());
                tag->err = err;
            }
        }
        {
            AQUIRE_MUTEX(restoreMtx)
            tag->restoreDone.store(true, boost::memory_order_seq_cst);
        }
        restoreCond.notify_one();
        return;
    }

    void delete_backup_handler(void *src, void *dest, uint64_t nbytes,
                               Fam_Delete_Backup_Tag *tag) {
        if (tag->memoryService) {
            try {
                tag->memoryService->delete_backup(tag->BackupName);
            } catch (Fam_Exception &e) {
                Fam_Async_Err *err = new Fam_Async_Err();
                err->set_error_code((Fam_Error)e.fam_error());
                err->set_error_msg(e.fam_error_msg());
                tag->err = err;
            }
        }
        {
            AQUIRE_MUTEX(deletebackupMtx)
            tag->delbackupDone.store(true, boost::memory_order_seq_cst);
        }
        deletebackupCond.notify_one();
        return;
    }

  private:
    boost::lockfree::queue<Fam_Ops_Info> *queue;
    boost::lockfree::queue<Fam_Async_Err *> *readCQ, *writeCQ, *copyCQ,
        *backupCQ, *restoreCQ, *deleteCQ;
    boost::thread_group consumerThreads;
#ifdef USE_BOOST_FIBER
    boost::fibers::condition_variable readCond, writeCond, copyCond, queueCond,
        backupCond, restoreCond, deletebackupCond;
    boost::fibers::mutex readMtx, writeMtx, copyMtx, queueMtx, backupMtx,
        restoreMtx, deletebackupMtx;
#else
    std::condition_variable readCond, writeCond, copyCond, queueCond,
        backupCond, restoreCond, deletebackupCond;
    std::mutex readMtx, writeMtx, copyMtx, queueMtx, backupMtx, restoreMtx,
        deletebackupMtx;
#endif
    boost::atomic_uint64_t readCtr, writeCtr, readErrCtr, writeErrCtr,
        copyErrCtr, backupErrCtr, restoreErrCtr, deleteErrCtr, qwriteCtr,
        qreadCtr;
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

uint64_t Fam_Async_QHandler::progress(Fam_Context *famCtx) {
    return fAsyncQHandler_->progress(famCtx);
}

uint64_t Fam_Async_QHandler::write_progress(Fam_Context *famCtx) {
    return fAsyncQHandler_->write_progress(famCtx);
}

uint64_t Fam_Async_QHandler::read_progress(Fam_Context *famCtx) {
    return fAsyncQHandler_->read_progress(famCtx);
}
void Fam_Async_QHandler::wait_for_copy(void *waitObj) {
    fAsyncQHandler_->wait_for_copy(waitObj);
}

void Fam_Async_QHandler::wait_for_backup(void *waitObj) {
    fAsyncQHandler_->wait_for_backup(waitObj);
}

void Fam_Async_QHandler::wait_for_restore(void *waitObj) {
    fAsyncQHandler_->wait_for_restore(waitObj);
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
                                      Fam_Copy_Tag *tag) {
    fAsyncQHandler_->copy_handler(src, dest, nbytes, tag);
}

void Fam_Async_QHandler::backup_handler(void *src, void *dest, uint64_t nbytes,
                                        Fam_Backup_Tag *tag) {
    fAsyncQHandler_->backup_handler(src, dest, nbytes, tag);
}

void Fam_Async_QHandler::restore_handler(void *src, void *dest, uint64_t nbytes,
                                         Fam_Restore_Tag *tag) {
    fAsyncQHandler_->restore_handler(src, dest, nbytes, tag);
}

void Fam_Async_QHandler::delete_backup_handler(void *src, void *dest,
                                               uint64_t nbytes,
                                               Fam_Delete_Backup_Tag *tag) {
    fAsyncQHandler_->delete_backup_handler(src, dest, nbytes, tag);
}

void Fam_Async_QHandler::wait_for_delete_backup(void *waitObj) {
    fAsyncQHandler_->wait_for_delete_backup(waitObj);
}

} // namespace openfam

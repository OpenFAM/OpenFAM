/*
 * fam_rpc_client.h
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

#ifndef FAM_RPC_CLIENT_H
#define FAM_RPC_CLIENT_H

#include <grpc/impl/codegen/log.h>
#include <grpcpp/grpcpp.h>
#include <iostream>
#include <sstream>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "common/fam_internal.h"
#include "fam/fam.h"
#include "fam/fam_exception.h"
#include "rpc/fam_rpc.grpc.pb.h"
using namespace std;

#define FAM_UNIMPLEMENTED_RPC()                                                \
    {                                                                          \
        cout << "returned from server..." << __func__                          \
             << " is Not Yet Implemented...!!!" << endl;                       \
    }
namespace openfam {

/**
 * structure for keeping state and data information of fam copy
 */
typedef struct {
    // Container for the data we expect from the server.
    Fam_Copy_Response res;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ::grpc::ClientContext ctx;

    // Storage for the status of the RPC upon completion.
    ::grpc::Status status;

    // falg for completion
    bool isCompleted;

    uint64_t memServerId;

    std::unique_ptr<::grpc::ClientAsyncResponseReader<Fam_Copy_Response>>
        responseReader;
} Fam_Copy_Tag;

class Fam_Rpc_Client {
  public:
    Fam_Rpc_Client(const char *name, uint64_t port) {
        std::ostringstream message;
        std::string name_s(name);
        name_s += ":" + std::to_string(port);

        uid = (uint32_t)getuid();
        gid = (uint32_t)getgid();

        /** Creating a channel and stub **/
        this->stub = Fam_Rpc::NewStub(
            grpc::CreateChannel(name_s, ::grpc::InsecureChannelCredentials()));

        if (!this->stub) {
            message << "Fam Grpc Initialialization failed: stub is null";
            throw Fam_Allocator_Exception(FAM_ERR_GRPC, message.str().c_str());
        }

        Fam_Request req;
        Fam_Start_Response res;

        ::grpc::ClientContext ctx;
        /** sending a start signal to server **/
        ::grpc::Status status = stub->signal_start(&ctx, req, &res);
        if (!status.ok()) {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
        memServerFabricAddrSize = res.addrnamelen();
        memServerFabricAddr = (char *)calloc(1, memServerFabricAddrSize);

        uint32_t lastBytes = 0;
        int lastBytesCount = (int)(memServerFabricAddrSize % sizeof(uint32_t));
        int readCount = res.addrname_size();

        if (lastBytesCount > 0)
            readCount -= 1;

        for (int ndx = 0; ndx < readCount; ndx++) {
            *((uint32_t *)memServerFabricAddr + ndx) = res.addrname(ndx);
        }

        if (lastBytesCount > 0) {
            lastBytes = res.addrname(readCount);
            memcpy(((uint32_t *)memServerFabricAddr + readCount), &lastBytes,
                   lastBytesCount);
        }
    }

    ~Fam_Rpc_Client() {
        Fam_Request req;
        Fam_Response res;

        ::grpc::ClientContext ctx;

        ::grpc::Status status = stub->signal_termination(&ctx, req, &res);

        free(memServerFabricAddr);
    }

    void reset_profile() {
#ifdef MEMSERVER_PROFILE
        Fam_Request req;
        Fam_Response res;

        ::grpc::ClientContext ctx;

        ::grpc::Status status = stub->reset_profile(&ctx, req, &res);
#endif
    }

    void generate_profile() {
#ifdef MEMSERVER_PROFILE
        Fam_Request req;
        Fam_Response res;

        ::grpc::ClientContext ctx;

        ::grpc::Status status = stub->generate_profile(&ctx, req, &res);
#endif
    }
    /**
     * Creates a Region in FAM
     * @param name-Name of the reion to be created
     * @param nbytes - size of the region
     * @param permissions - Permission with which the region needs to be created
     * @param redundancyLevel - Redundancy level of FAM
     * @return - pointer to Fam_Region_Descriptor
     * @see fam_rpc.proto
     **/
    Fam_Region_Descriptor *create_region(const char *name, size_t nbytes,
                                         mode_t permission,
                                         Fam_Redundancy_Level redundancyLevel,
                                         uint64_t memoryServerId) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;

        req.set_name(name);
        req.set_size(nbytes);
        req.set_perm(permission);
        req.set_uid(uid);
        req.set_gid(gid);
        ::grpc::Status status = stub->create_region(&ctx, req, &res);
        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                Fam_Global_Descriptor globalDescriptor;

                globalDescriptor.regionId =
                    res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
                globalDescriptor.offset = res.offset();
                Fam_Region_Descriptor *region =
                    new Fam_Region_Descriptor(globalDescriptor, nbytes);
                return region;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * Destroys a Region in FAM
     * @param region - Descriptor of a region to be destroyed
     * @see fam_rpc.proto
     **/
    void destroy_region(Fam_Region_Descriptor *region) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            region->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status = stub->destroy_region(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * Resize a Region existing in FAM
     * @param region - Fam region descriptor of a region which needs to be
     *resized
     * @param nbytes - new size of the region
     * @return - 1/0
     * @see fam_rpc.proto
     **/
    int resize_region(Fam_Region_Descriptor *region, size_t nbytes) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            region->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_size(nbytes);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status = stub->resize_region(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return FAM_SUCCESS;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * Allocate data item within the specified region
     * @param name - name of the data item
     * @param namelen - no. charecter in name
     * @param nbytes - size of the data item
     * @param region - Fam region descriptor of a region within which data
     *item needs to be created
     * @return -  pointer to Fam_Descriptor
     * @see fam_rpc.proto
     **/
    Fam_Descriptor *allocate(const char *name, size_t nbytes, mode_t permission,
                             Fam_Region_Descriptor *region) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            region->get_global_descriptor();
        req.set_name(name);
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_size(nbytes);
        req.set_perm(permission);
        req.set_uid(uid);
        req.set_gid(gid);
        req.set_dup(false);
        uint64_t nodeId = region->get_memserver_id();

        ::grpc::Status status = stub->allocate(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                globalDescriptor.regionId =
                    res.regionid() | (nodeId << MEMSERVERID_SHIFT);
                globalDescriptor.offset = res.offset();
                Fam_Descriptor *dataItem =
                    new Fam_Descriptor(globalDescriptor, nbytes);
                dataItem->bind_key(res.key());
                dataItem->set_base_address((void *)res.base());
                return dataItem;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * deallocates the data item
     * @param dataitem - Fam data item descriptor which needs to be
     *deallocated
     * @see fam_rpc.proto
     **/
    void deallocate(Fam_Descriptor *dataitem) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            dataitem->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_offset(globalDescriptor.offset);
        req.set_uid(uid);
        req.set_gid(gid);
        req.set_key(dataitem->get_key());

        ::grpc::Status status = stub->deallocate(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * Chenages the permission of a region
     * @param desc - Descriptor of a region whose permissions needs to be
     *changed
     * @param permission - new permission
     **/
    int change_permission(Fam_Region_Descriptor *region, mode_t permission) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            region->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_perm((uint64_t)permission);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status = stub->change_region_permission(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return FAM_SUCCESS;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    /**
     * Chenages the permission of a dataitem
     * @param desc - Descriptor of a dataitem whose permissions needs to be
     *changed
     * @param permission - new permission
     **/
    int change_permission(Fam_Descriptor *dataitem, mode_t permission) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            dataitem->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_offset(globalDescriptor.offset);
        req.set_perm((uint64_t)permission);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status =
            stub->change_dataitem_permission(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return FAM_SUCCESS;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    Fam_Region_Descriptor *lookup_region(const char *name,
                                         uint64_t memoryServerId) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;

        req.set_name(name);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status = stub->lookup_region(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                Fam_Global_Descriptor globalDescriptor;
                globalDescriptor.regionId =
                    res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
                globalDescriptor.offset = res.offset();
                Fam_Region_Descriptor *region =
                    new Fam_Region_Descriptor(globalDescriptor, res.size());
                return region;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    Fam_Descriptor *lookup(const char *itemName, const char *regionName,
                           uint64_t memoryServerId) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        req.set_name(itemName);
        req.set_regionname(regionName);
        req.set_uid(uid);
        req.set_gid(gid);

        ::grpc::Status status = stub->lookup(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                Fam_Global_Descriptor globalDescriptor;
                globalDescriptor.regionId =
                    res.regionid() | (memoryServerId << MEMSERVERID_SHIFT);
                globalDescriptor.offset = res.offset();
                Fam_Descriptor *dataItem =
                    new Fam_Descriptor(globalDescriptor, res.size());
                dataItem->bind_key(FAM_KEY_UNINITIALIZED);
                return dataItem;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    Fam_Region_Item_Info
    check_permission_get_info(Fam_Region_Descriptor *region) {
        Fam_Region_Request req;
        Fam_Region_Response res;
        ::grpc::ClientContext ctx;
        Fam_Region_Item_Info regionInfo;

        Fam_Global_Descriptor globalDescriptor =
            region->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_gid(gid);
        req.set_uid(uid);

        ::grpc::Status status =
            stub->check_permission_get_region_info(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                regionInfo.size = res.size();
                return regionInfo;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    Fam_Region_Item_Info check_permission_get_info(Fam_Descriptor *dataitem) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;
        Fam_Region_Item_Info itemInfo;

        Fam_Global_Descriptor globalDescriptor =
            dataitem->get_global_descriptor();
        req.set_regionid(globalDescriptor.regionId & REGIONID_MASK);
        req.set_offset(globalDescriptor.offset);
        req.set_gid(gid);
        req.set_uid(uid);

        ::grpc::Status status =
            stub->check_permission_get_item_info(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                itemInfo.key = res.key();
                itemInfo.size = res.size();
                itemInfo.base = (void *)res.base();
                return itemInfo;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    void *copy(Fam_Descriptor *src, uint64_t srcOffset, Fam_Descriptor **dest,
               uint64_t destOffset, uint64_t nbytes) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        Fam_Copy_Request copyReq;
        Fam_Copy_Response copyRes;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor srcGlobalDescriptor =
            src->get_global_descriptor();
        req.set_regionid(srcGlobalDescriptor.regionId & REGIONID_MASK);
        req.set_offset(srcGlobalDescriptor.offset);
        req.set_gid(gid);
        req.set_uid(uid);
        req.set_dup(true);

        uint64_t itemSize = src->get_size();

        if ((srcOffset + nbytes) > itemSize) {
            throw Fam_Allocator_Exception(
                FAM_ERR_OUTOFRANGE,
                "Source offset or size is beyond dataitem boundary");
        }

        if ((destOffset + nbytes) > itemSize) {
            throw Fam_Allocator_Exception(
                FAM_ERR_OUTOFRANGE,
                "Destination offset or size is beyond dataitem boundary");
        }

        ::grpc::Status status = stub->allocate(&ctx, req, &res);

        Fam_Global_Descriptor destGlobalDescriptor;
        uint64_t nodeId = src->get_memserver_id();

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                destGlobalDescriptor.regionId =
                    res.regionid() | (nodeId << MEMSERVERID_SHIFT);
                destGlobalDescriptor.offset = res.offset();
                *dest = new Fam_Descriptor(destGlobalDescriptor, res.size());
                (*dest)->bind_key(res.key());
                (*dest)->set_base_address((void *)res.base());
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }

        copyReq.set_regionid(srcGlobalDescriptor.regionId & REGIONID_MASK);
        copyReq.set_srcoffset(srcGlobalDescriptor.offset);
        copyReq.set_destoffset(destGlobalDescriptor.offset);
        copyReq.set_srccopystart(srcOffset);
        copyReq.set_destcopystart(destOffset);
        copyReq.set_gid(gid);
        copyReq.set_uid(uid);
        copyReq.set_copysize(nbytes);

        Fam_Copy_Tag *tag = new Fam_Copy_Tag();

        tag->isCompleted = false;
        tag->memServerId = nodeId;

        tag->responseReader = stub->PrepareAsynccopy(&tag->ctx, copyReq, &cq);

        // StartCall initiates the RPC call
        tag->responseReader->StartCall();

        tag->responseReader->Finish(&tag->res, &tag->status, (void *)tag);

        return (void *)tag;
    }

    void wait_for_copy(void *waitObj) {
        void *got_tag;
        bool ok = false;
        Fam_Copy_Tag *tagIn, *tagCompleted;
        ::grpc::Status status;
        Fam_Copy_Response res;

        tagIn = static_cast<Fam_Copy_Tag *>(waitObj);

        if (!tagIn) {
            throw Fam_Allocator_Exception(FAM_ERR_INVALID, "Copy tag is null");
        }

        if (tagIn->isCompleted) {
            if (tagIn->status.ok()) {
                if (tagIn->res.errorcode()) {
                    res = tagIn->res;
                    delete tagIn;
                    throw Fam_Allocator_Exception(
                        (enum Fam_Error)res.errorcode(),
                        res.errormsg().c_str());
                } else {
                    delete tagIn;
                    return;
                }
            } else {
                status = tagIn->status;
                delete tagIn;
                throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                              (status.error_message()).c_str());
            }
        } else {

            do {
                GPR_ASSERT(cq.Next(&got_tag, &ok));
                // The tag is the memory location of Fam_Copy_Tag object
                tagCompleted = static_cast<Fam_Copy_Tag *>(got_tag);
                if (!tagCompleted) {
                    throw Fam_Allocator_Exception(FAM_ERR_INVALID,
                                                  "Copy tag is null");
                }
                tagCompleted->isCompleted = true;
                // Verify that the request was completed successfully. Note
                // that "ok" corresponds solely to the request for updates
                // introduced by Finish().
                GPR_ASSERT(ok);
            } while (tagIn != tagCompleted);

            if (tagCompleted->status.ok()) {
                if (tagCompleted->res.errorcode()) {
                    res = tagCompleted->res;
                    delete tagCompleted;
                    throw Fam_Allocator_Exception(
                        (enum Fam_Error)res.errorcode(),
                        res.errormsg().c_str());
                } else {
                    delete tagCompleted;
                    return;
                }
            } else {
                status = tagCompleted->status;
                delete tagCompleted;
                throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                              (status.error_message()).c_str());
            }
        }
    }

    void acquire_CAS_lock(Fam_Descriptor *dataitem) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            dataitem->get_global_descriptor();
        req.set_offset(globalDescriptor.offset);

        ::grpc::Status status = stub->acquire_CAS_lock(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    void release_CAS_lock(Fam_Descriptor *dataitem) {
        Fam_Dataitem_Request req;
        Fam_Dataitem_Response res;
        ::grpc::ClientContext ctx;

        Fam_Global_Descriptor globalDescriptor =
            dataitem->get_global_descriptor();
        req.set_offset(globalDescriptor.offset);

        ::grpc::Status status = stub->release_CAS_lock(&ctx, req, &res);

        if (status.ok()) {
            if (res.errorcode()) {
                throw Fam_Allocator_Exception((enum Fam_Error)res.errorcode(),
                                              (res.errormsg()).c_str());
            } else {
                return;
            }
        } else {
            throw Fam_Allocator_Exception(FAM_ERR_GRPC,
                                          (status.error_message()).c_str());
        }
    }

    size_t get_addr_size() { return memServerFabricAddrSize; };
    char *get_addr() { return memServerFabricAddr; };

  private:
    std::unique_ptr<Fam_Rpc::Stub> stub;
    uint32_t uid;
    uint32_t gid;

    size_t memServerFabricAddrSize;
    char *memServerFabricAddr;

    ::grpc::CompletionQueue cq;
};

} // namespace openfam
#endif

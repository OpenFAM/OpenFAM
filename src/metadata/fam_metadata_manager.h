/*
 *   fam_metadata_manager.h
 *   Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *      IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *      BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *      FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *      INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *      DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *      OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *      OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *      IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

#ifndef FAM_METADATA_MANAGER_H
#define FAM_METADATA_MANAGER_H

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <map>

#include "radixtree/kvs.h"
#include "radixtree/radix_tree.h"

#include "common/fam_internal.h"
#include "common/fam_internal_exception.h"
#include "nvmm/epoch_manager.h"
#include "nvmm/fam.h"
#include "nvmm/memory_manager.h"

using namespace famradixtree;
using namespace nvmm;
using namespace std;

namespace metadata {

#ifdef META_DEBUG
#define DEBUG_STDOUT(x, y)                                                     \
    {                                                                          \
        std::cout << "[Debug: " << __FUNCTION__ << " : " << __LINE__ << "] "   \
                  << x << " : " << y << std::endl;                             \
    }
#define DEBUG_STDERR(x, y)                                                     \
    {                                                                          \
        std::cerr << "[Error: " << __FUNCTION__ << " : " << __LINE__ << "] "   \
                  << x << " : " << y << std::endl;                             \
    }
#else
#define DEBUG_STDOUT(x, y)
#define DEBUG_STDERR(x, y)
#endif // META_DEBUG

#define METADATA_HEAP_ID 16
#define METADATA_HEAP_SIZE (1024 * 1024 * 1024)

typedef struct {
    KeyValueStore *diIdKVS;
    KeyValueStore *diNameKVS;
} diKVS;

using KvsMap = std::map<uint64_t, diKVS *>;

/**
 * Region Metadata descriptor
 */
typedef struct {
    /**
     * Region ID for this descriptor
     */
    uint64_t regionId;
    /*
     * Region    : REGION_OFFSET(-1)/Invalid offset
     */
    uint64_t offset;
    uint32_t uid;
    uint32_t gid;
    mode_t perm;
    char name[RadixTree::MAX_KEY_LEN];
    uint64_t size;
    //   Fam_Redundancy_Level redundancyLevel;
    GlobalPtr dataItemIdRoot;
    GlobalPtr dataItemNameRoot;
} Fam_Region_Metadata;

/**
 *  DataItem Metadata descriptor
 */
typedef struct {
    uint64_t regionId;
    /**
     * Data Item : Offset within the region for the start of the memory
     *             representing the descriptor
     */
    uint64_t offset;
    uint32_t uid;
    uint32_t gid;
    mode_t perm;
    char name[RadixTree::MAX_KEY_LEN];
    uint64_t size;
} Fam_DataItem_Metadata;

typedef enum metadata_region_item_op {
    META_REGION_ITEM_WRITE = 1 << 0,
    META_REGION_ITEM_READ = 1 << 1,
    META_REGION_ITEM_EXEC = 1 << 2,
    META_REGION_ITEM_RW = META_REGION_ITEM_WRITE | META_REGION_ITEM_READ
} metadata_region_item_op_t;

enum metadata_ErrorVal {
    META_KEY_ALREADY_EXIST = -3,
    META_KEY_DOES_NOT_EXIST = -2,
    META_ERROR = -1,
    META_NO_ERROR = 0
};

class Fam_Metadata_Manager {
  public:
    virtual ~Fam_Metadata_Manager(){};

    virtual void reset_profile() = 0;

    virtual void dump_profile() = 0;

    virtual void metadata_insert_region(const uint64_t regionId,
                                        const std::string regionName,
                                        Fam_Region_Metadata *region) = 0;

    virtual void metadata_delete_region(const uint64_t regionId) = 0;

    virtual void metadata_delete_region(const std::string regionName) = 0;

    virtual bool metadata_find_region(const uint64_t regionId,
                                      Fam_Region_Metadata &region) = 0;
    virtual bool metadata_find_region(const std::string regionName,
                                      Fam_Region_Metadata &region) = 0;

    virtual void metadata_modify_region(const uint64_t regionID,
                                        Fam_Region_Metadata *region) = 0;
    virtual void metadata_modify_region(const std::string regionName,
                                        Fam_Region_Metadata *region) = 0;

    virtual void metadata_insert_dataitem(const uint64_t dataitemId,
                                          const uint64_t regionId,
                                          Fam_DataItem_Metadata *dataitem,
                                          std::string dataitemName = "") = 0;

    virtual void metadata_insert_dataitem(const uint64_t dataitemId,
                                          const std::string regionName,
                                          Fam_DataItem_Metadata *dataitem,
                                          std::string dataitemName = "") = 0;

    virtual void metadata_delete_dataitem(const uint64_t dataitemId,
                                          const std::string regionName) = 0;
    virtual void metadata_delete_dataitem(const uint64_t dataitemId,
                                          const uint64_t regionId) = 0;
    virtual void metadata_delete_dataitem(const std::string dataitemName,
                                          const std::string regionName) = 0;
    virtual void metadata_delete_dataitem(const std::string dataitemName,
                                          const uint64_t regionId) = 0;

    virtual bool metadata_find_dataitem(const uint64_t dataitemId,
                                        const uint64_t regionId,
                                        Fam_DataItem_Metadata &dataitem) = 0;
    virtual bool metadata_find_dataitem(const uint64_t dataitemId,
                                        const std::string regionName,
                                        Fam_DataItem_Metadata &dataitem) = 0;
    virtual bool metadata_find_dataitem(const std::string dataitemName,
                                        const uint64_t regionId,
                                        Fam_DataItem_Metadata &dataitem) = 0;
    virtual bool metadata_find_dataitem(const std::string dataitemName,
                                        const std::string regionName,
                                        Fam_DataItem_Metadata &dataitem) = 0;

    virtual void metadata_modify_dataitem(const uint64_t dataitemId,
                                          const uint64_t regionId,
                                          Fam_DataItem_Metadata *dataitem) = 0;
    virtual void metadata_modify_dataitem(const uint64_t dataitemId,
                                          const std::string regionName,
                                          Fam_DataItem_Metadata *dataitem) = 0;
    virtual void metadata_modify_dataitem(const std::string dataitemName,
                                          const uint64_t regionId,
                                          Fam_DataItem_Metadata *dataitem) = 0;
    virtual void metadata_modify_dataitem(const std::string dataitemName,
                                          const std::string regionName,
                                          Fam_DataItem_Metadata *dataitem) = 0;

    virtual bool metadata_check_permissions(Fam_DataItem_Metadata *dataitem,
                                            metadata_region_item_op_t op,
                                            uint32_t uid, uint32_t gid) = 0;

    virtual bool metadata_check_permissions(Fam_Region_Metadata *region,
                                            metadata_region_item_op_t op,
                                            uint32_t uid, uint32_t gid) = 0;
    virtual size_t metadata_maxkeylen() = 0;
};

} // namespace metadata
#endif

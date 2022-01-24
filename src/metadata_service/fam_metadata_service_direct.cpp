/*
 *   fam_metadata_service_direct.cpp
 *   Copyright (c) 2019-2020 Hewlett Packard Enterprise Development, LP. All
 *   rights reserved.
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
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

#include "fam_metadata_service_direct.h"

#include <boost/atomic.hpp>

#include <atomic>
#include <bits/stdc++.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include "bitmap-manager/bitmap.h"
#include "common/fam_internal.h"
#include "common/fam_memserver_profile.h"

#define OPEN_METADATA_KVS(root, heap_size, heap_id, kvs)                       \
    if (use_meta_region) {                                                     \
        kvs = open_metadata_kvs(root);                                         \
    } else {                                                                   \
        kvs = open_metadata_kvs(root, heap_size, heap_id);                     \
    }

using namespace radixtree;
using namespace openfam;
using namespace nvmm;
using namespace std;
using namespace chrono;

namespace metadata {
MEMSERVER_PROFILE_START(METADATA_DIRECT)
#ifdef MEMSERVER_PROFILE
#define METADATA_DIRECT_PROFILE_START_OPS()                                    \
    {                                                                          \
        Profile_Time start = METADATA_DIRECT_get_time();

#define METADATA_DIRECT_PROFILE_END_OPS(apiIdx)                                \
    Profile_Time end = METADATA_DIRECT_get_time();                             \
    Profile_Time total = METADATA_DIRECT_time_diff_nanoseconds(start, end);    \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA_DIRECT, prof_##apiIdx, total)  \
    }
#define METADATA_DIRECT_PROFILE_DUMP() metadata_direct_profile_dump()
#else
#define METADATA_DIRECT_PROFILE_START_OPS()
#define METADATA_DIRECT_PROFILE_END_OPS(apiIdx)
#define METADATA_DIRECT_PROFILE_DUMP()
#endif

void metadata_direct_profile_dump() {
    MEMSERVER_PROFILE_END(METADATA_DIRECT)
    MEMSERVER_DUMP_PROFILE_BANNER(METADATA_DIRECT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_DIRECT, name, prof_##name)
#include "metadata_service/metadata_direct_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_DIRECT, prof_##name)
#include "metadata_service/metadata_direct_counters.tbl"
    MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_DIRECT)
}

KeyValueStore::IndexType const KVSTYPE = KeyValueStore::RADIX_TREE;

size_t const max_val_len = 4096;

inline void ResetBuf(char *buf, size_t &len, size_t const max_len) {
    memset(buf, 0, max_len);
    len = max_len;
}

/*
 * Internal implementation of Fam_Metadata_Service_Direct
 */
class Fam_Metadata_Service_Direct::Impl_ {
  public:
    Impl_() {}

    ~Impl_() {}

    int Init(bool use_meta_reg, bool enable_region_spanning,
             size_t region_span_size_per_memoryserver);

    int Final();

    void metadata_insert_region(const uint64_t regionId,
                                const std::string regionName,
                                Fam_Region_Metadata *region);

    void metadata_delete_region(const uint64_t regionId);

    void metadata_delete_region(const std::string regionName);

    bool metadata_find_region(const uint64_t regionId,
                              Fam_Region_Metadata &region);

    bool metadata_find_region(const std::string regionName,
                              Fam_Region_Metadata &region);

    void metadata_modify_region(const uint64_t regionID,
                                Fam_Region_Metadata *region);

    void metadata_modify_region(const std::string regionName,
                                Fam_Region_Metadata *region);

    void metadata_insert_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem,
                                  std::string dataitemName = "");

    void metadata_insert_dataitem(const uint64_t dataitemId,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem,
                                  std::string dataitemName = "");

    void metadata_delete_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId);

    void metadata_delete_dataitem(const uint64_t dataitemId,
                                  const std::string regionName);

    void metadata_delete_dataitem(const std::string dataitemName,
                                  const uint64_t regionId);

    void metadata_delete_dataitem(const std::string dataitemName,
                                  const std::string regionName);

    bool metadata_find_dataitem(const uint64_t dataitemId,
                                const uint64_t regionId,
                                Fam_DataItem_Metadata &dataitem);

    bool metadata_find_dataitem(const uint64_t dataitemId,
                                const std::string regionName,
                                Fam_DataItem_Metadata &dataitem);

    bool metadata_find_dataitem(const std::string dataitemName,
                                const uint64_t regionId,
                                Fam_DataItem_Metadata &dataitem);

    bool metadata_find_dataitem(const std::string dataitemName,
                                const std::string regionName,
                                Fam_DataItem_Metadata &dataitem);

    void metadata_modify_dataitem(const uint64_t dataitemId,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem);

    void metadata_modify_dataitem(const uint64_t dataitemId,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem);

    void metadata_modify_dataitem(const std::string dataitemName,
                                  const uint64_t regionId,
                                  Fam_DataItem_Metadata *dataitem);

    void metadata_modify_dataitem(const std::string dataitemName,
                                  const std::string regionName,
                                  Fam_DataItem_Metadata *dataitem);

    bool metadata_check_permissions(Fam_DataItem_Metadata *dataitem,
                                    metadata_region_item_op_t op, uint32_t uid,
                                    uint32_t gid);

    bool metadata_check_permissions(Fam_Region_Metadata *region,
                                    metadata_region_item_op_t op, uint32_t uid,
                                    uint32_t gid);

    void metadata_validate_and_create_region(
        string regionname, size_t size, uint64_t *regionid,
        Fam_Region_Attributes *regionAttributes,
        std::list<int> *memory_server_list, int user_policy);

    void
    metadata_validate_and_destroy_region(const uint64_t regionId, uint32_t uid,
                                         uint32_t gid,
                                         std::list<int> *memory_server_list);
    void metadata_validate_and_allocate_dataitem(const std::string dataitemName,
                                                 const uint64_t regionId,
                                                 uint32_t uid, uint32_t gid,
                                                 uint64_t *memoryServerId);

    void metadata_validate_and_deallocate_dataitem(const uint64_t regionId,
                                                   const uint64_t dataitemId,
                                                   uint32_t uid, uint32_t gid);
    size_t metadata_maxkeylen();
    void metadata_update_memoryserver(
        int nmemServersPersistent,
        std::vector<uint64_t> memsrv_persistent_id_list,
        int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list);
    void metadata_reset_bitmap(uint64_t regionID);
    uint64_t align_to_address(uint64_t size, int multiple);
    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
        uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_region_and_check_permissions(
        metadata_region_item_op_t op, const std::string regionName,
        uint32_t uid, uint32_t gid, Fam_Region_Metadata &region);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t dataitemId,
        const uint64_t regionId, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    void metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const std::string dataitemName,
        const std::string regionName, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem);

    std::list<int> get_memory_server_list(uint64_t regionId);

  private:
    // KVS for region Id tree
    KeyValueStore *regionIdKVS;
    // KVS for region Name tree
    KeyValueStore *regionNameKVS;
    // KVS root pointer for region Id tree
    GlobalPtr regionIdRoot;
    // KVS root pointer for region Name tree
    GlobalPtr regionNameRoot;
    bitmap *bmap;
    std::vector<uint64_t> memoryServerPersistentList;
    std::vector<uint64_t> memoryServerVolatileList;
    bool use_meta_region;
    KvsMap *metadataKvsMap;
    pthread_rwlock_t kvsMapLock;
    pthread_mutex_t memserverListLock;

    MemoryManager *memoryManager;
    bool enable_region_spanning;
    size_t region_span_size_per_memoryserver;

    GlobalPtr create_metadata_kvs_tree(size_t heap_size = METADATA_HEAP_SIZE,
                                       nvmm::PoolId heap_id = METADATA_HEAP_ID);

    KeyValueStore *open_metadata_kvs(GlobalPtr root,
                                     size_t heap_size = METADATA_HEAP_SIZE,
                                     uint64_t heap_id = METADATA_HEAP_ID);

    int get_dataitem_KVS(uint64_t regionId, KeyValueStore *&dataitemIdKVS,
                         KeyValueStore *&dataitemNameKVS,
                         pthread_rwlock_t **kvsLock);

    void release_kvs_lock(pthread_rwlock_t *kvsLock);

    int insert_in_regionname_kvs(const std::string regionName,
                                 const std::string regionId);

    int insert_in_regionid_kvs(const std::string regionKey,
                               Fam_Region_Metadata *region, bool insert);

    int get_regionid_from_regionname_KVS(const std::string regionName,
                                         std::string &regionId);
    void init_poolid_bmap();

    std::list<int> find_memory_server_list(const std::string regionName,
                                           size_t size,
                                           Fam_Memory_Type memoryType,
                                           int user_policy);

    std::list<int> find_memory_server_list(Fam_Region_Metadata region);

    int get_regionid_from_bitmap(uint64_t *regionID);
    bool create_heap_for_dataitem_metadata_KVS(const uint64_t regionId,
                                               size_t heap_size);
    void destroy_dataitem_metadata_KVS(const uint64_t regionId,
                                       Fam_Region_Metadata regNode);
};

/*
 * Initialize the FAM metadata manager
 */
int Fam_Metadata_Service_Direct::Impl_::Init(bool use_meta_reg, bool flag,
                                             size_t size) {

    memoryManager = MemoryManager::GetInstance();
    enable_region_spanning = flag;
    region_span_size_per_memoryserver = size;
    metadataKvsMap = new KvsMap();
    pthread_rwlock_init(&kvsMapLock, NULL);
    pthread_mutex_init(&memserverListLock, NULL);
    use_meta_region = use_meta_reg;

    // Create the KVS tree for Region ID
    // Get the regionIdRoot from NVMM root-shelf
    // If it's not present in NVMM root-shelf, create a new KVS and
    // store the GlobalPtr in NVMM root-shelf
    regionIdRoot = memoryManager->GetMetadataRegionRootPtr(METADATA_REGION_ID);
    if (regionIdRoot == 0) {
        regionIdRoot = create_metadata_kvs_tree();
        regionIdRoot = memoryManager->SetMetadataRegionRootPtr(
            METADATA_REGION_ID, regionIdRoot);
    }

    // Open the region Id KVS tree
    regionIdKVS = open_metadata_kvs(regionIdRoot);
    if (regionIdKVS == nullptr) {
        DEBUG_STDERR("Metadata Init", "KVS creation failed.");
        return META_ERROR;
    }

    // Create the KVS tree for storing region name to region ID mapping
    regionNameRoot =
        memoryManager->GetMetadataRegionRootPtr(METADATA_REGION_NAME);
    if (regionNameRoot == 0) {
        regionNameRoot = create_metadata_kvs_tree();
        regionNameRoot = memoryManager->SetMetadataRegionRootPtr(
            METADATA_REGION_NAME, regionNameRoot);
    }

    // Open the region name KVS tree
    regionNameKVS = open_metadata_kvs(regionNameRoot);
    if (regionNameKVS == nullptr) {
        DEBUG_STDERR("Metadata Init", "KVS creation failed.");
        return META_ERROR;
    }
    // Initialize bitmap
    init_poolid_bmap();

    // TODO: Read memoryServerCount from KVS
    // As of now set memory server count as 1 in init.

    return META_NO_ERROR;
}

int Fam_Metadata_Service_Direct::Impl_::Final() {
    // Cleanup
    delete regionIdKVS;
    delete regionNameKVS;
    delete metadataKvsMap;
    pthread_rwlock_destroy(&kvsMapLock);
    pthread_mutex_destroy(&memserverListLock);

    return META_NO_ERROR;
}

/*
 * Initalize the region Id bitmap address.
 * Size of bitmap is Max poolId's supported / 8 bytes.
 * Get the Bitmap address reserved from the root shelf.
 */
void Fam_Metadata_Service_Direct::Impl_::init_poolid_bmap() {
    bmap = new bitmap();
    bmap->size = (ShelfId::kMaxPoolCount * BITSIZE) / sizeof(uint64_t);
    bmap->map = memoryManager->GetRegionIdBitmapAddr();
}

/**
 * create_metadata_kvs_tree - Helper function to create a KVS radixtree
 * 	for storing  metadata for the region and the dataitems
 * @return - Global pointer to the KVS tree root
 */
GlobalPtr
Fam_Metadata_Service_Direct::Impl_::create_metadata_kvs_tree(size_t heap_size,
                                                             PoolId heap_id) {

    KeyValueStore *metadataKVS;
    metadataKVS =
        KeyValueStore::MakeKVS(KVSTYPE, 0, "", "", heap_size, heap_id);

    if (metadataKVS == nullptr) {
        return META_ERROR;
    }

    GlobalPtr metadataRoot = metadataKVS->Location();

    delete metadataKVS;

    return metadataRoot;
}

KeyValueStore *Fam_Metadata_Service_Direct::Impl_::open_metadata_kvs(
    GlobalPtr root, size_t heap_size, uint64_t heap_id) {
    return KeyValueStore::MakeKVS(KVSTYPE, root, "", "", heap_size,
                                  (PoolId)heap_id);
}

int Fam_Metadata_Service_Direct::Impl_::get_dataitem_KVS(
    uint64_t regionId, KeyValueStore *&dataitemIdKVS,
    KeyValueStore *&dataitemNameKVS, pthread_rwlock_t **kvsLock) {

    int ret = META_NO_ERROR;
    *kvsLock = nullptr;
    pthread_rwlock_rdlock(&kvsMapLock);
    auto kvsObj = metadataKvsMap->find(regionId);
    if (kvsObj == metadataKvsMap->end()) {
        pthread_rwlock_unlock(&kvsMapLock);
        // If regionId is not present in KVS map, open the KVS using
        // root pointer and insert the KVS in the map
        Fam_Region_Metadata regNode;
        if (!metadata_find_region(regionId, regNode)) {
            return META_ERROR;
        }

        OPEN_METADATA_KVS(regNode.dataItemIdRoot, regNode.size, regionId,
                          dataitemIdKVS);
        if (dataitemIdKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            return META_ERROR;
        }

        OPEN_METADATA_KVS(regNode.dataItemNameRoot, regNode.size, regionId,
                          dataitemNameKVS);
        if (dataitemNameKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            return META_ERROR;
        }

        // Insert the KVS pointer into map
        diKVS *kvs = new diKVS();
        diKVS *kvsDiscard = NULL;
        kvs->diNameKVS = dataitemNameKVS;
        kvs->diIdKVS = dataitemIdKVS;
        pthread_rwlock_init(&kvs->kvsLock, NULL);
        pthread_rwlock_wrlock(&kvsMapLock);
        auto res = metadataKvsMap->insert({ regionId, kvs });
        pthread_rwlock_rdlock(&kvs->kvsLock);
        if (!res.second) {
            pthread_rwlock_unlock(&kvs->kvsLock);
            kvsDiscard = kvs;
            kvs = res.first->second;
            pthread_rwlock_rdlock(&kvs->kvsLock);
            dataitemIdKVS = kvs->diIdKVS;
            dataitemNameKVS = kvs->diNameKVS;
        }
        pthread_rwlock_unlock(&kvsMapLock);
        *kvsLock = &kvs->kvsLock;
        if (kvsDiscard) {
            pthread_rwlock_destroy(&kvsDiscard->kvsLock);
            delete kvsDiscard->diIdKVS;
            delete kvsDiscard->diNameKVS;
            delete kvsDiscard;
        }

    } else {
        pthread_rwlock_rdlock(&(kvsObj->second)->kvsLock);
        pthread_rwlock_unlock(&kvsMapLock);
        dataitemIdKVS = (kvsObj->second)->diIdKVS;
        dataitemNameKVS = (kvsObj->second)->diNameKVS;
        *kvsLock = &(kvsObj->second)->kvsLock;
    }

    if ((dataitemIdKVS == nullptr) || (dataitemNameKVS == nullptr)) {
        DEBUG_STDERR(regionId, "KVS not found");
        return META_ERROR;
    }

    return ret;
}

void Fam_Metadata_Service_Direct::Impl_::release_kvs_lock(
    pthread_rwlock_t *kvsLock) {
    if (kvsLock) {
        pthread_rwlock_unlock(kvsLock);
    }
}

/**
 * metadata_find_region - Lookup a region id in metadata region KVS
 * @param regionId - Region ID
 * @param region - return the Descriptor to the region if it exists
 * @return - true if key exists, and false if key not found.
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_region(
    const uint64_t regionId, Fam_Region_Metadata &region) {

    ostringstream message;
    int ret;
    std::string regionKey = std::to_string(regionId);
    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);
    ret =
        regionIdKVS->Get(regionKey.c_str(), regionKey.size(), val_buf, val_len);
    if (ret == META_NO_ERROR) {
        memcpy((char *)&region, val_buf, sizeof(Fam_Region_Metadata));
        return true;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return false;
    } else {
        DEBUG_STDERR(regionId, ret);
        message << "metadata find for region failed";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
    }
}

/**
 * metadata_find_region - Lookup a region name in region name to
 * 	region id KVS tree and then lookup the region metatdata from
 * 	region ID KVS
 * @param regionName - Region Name
 * @param region - return the Descriptor to the region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if
 * 	key not found
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_region(
    const std::string regionName, Fam_Region_Metadata &region) {
    ostringstream message;

    int ret;

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    ret = regionNameKVS->Get(regionName.c_str(), regionName.size(), val_buf,
                             val_len);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return false;
    } else if (ret == META_NO_ERROR) {

        uint64_t regionID = strtoul(val_buf, NULL, 0);

        return metadata_find_region(regionID, region);

    } else {
        DEBUG_STDERR(regionName, "Get failed.");
        message << "metadata find for region failed";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
    }
}

/**
 * insert_in_regionname_kvs - Helper function to create a region name to
 * 		region id KVS tree
 * @param regionName - Region Name
 * @param regionId - Region Id
 * @return - META_NO_ERROR if key added successfully, META_KEY_ALREADY_EXIST if
 * 	key already exists
 */
int Fam_Metadata_Service_Direct::Impl_::insert_in_regionname_kvs(
    const std::string regionName, const std::string regionId) {
    int ret = 0;

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    // Check if RegionName entry is already not present to avoid duplicate name
    // for region ids
    ret = regionNameKVS->FindOrCreate(regionName.c_str(), regionName.size(),
                                      regionId.c_str(), regionId.size(),
                                      val_buf, val_len);

    if (ret == META_NO_ERROR) {
        return ret;
    } else {
        DEBUG_STDERR(regionName, "FindOrCreate failed.");
        return ret;
    }
    return ret;
}

/**
 * insert_in_regionid_kvs - Helper function to add the region  descriptor
 * 		 to the region id KVS tree
 * @param regionName - Region Name
 * @param region - Region descriptor to be added
 * @param insert - bool flag insert -1 for insert; 0 for update
 * @return - META_NO_ERROR if key added successfully
 */
int Fam_Metadata_Service_Direct::Impl_::insert_in_regionid_kvs(
    const std::string regionName, Fam_Region_Metadata *region, bool insert) {

    int ret;
    char val_node[sizeof(Fam_Region_Metadata) + 1];
    memcpy((char *)val_node, (char const *)region, sizeof(Fam_Region_Metadata));

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    // Use FindOrCReate() for atomic metadata insert and Put() for
    // metadata modify.
    if (insert) {
        ret = regionIdKVS->FindOrCreate(regionName.c_str(), regionName.size(),
                                        val_node, sizeof(Fam_Region_Metadata),
                                        val_buf, val_len);
    } else {
        ret = regionIdKVS->Put(regionName.c_str(), regionName.size(), val_node,
                               sizeof(Fam_Region_Metadata));
    }

    if (ret == META_NO_ERROR) {
        return ret;
    } else if (ret == META_KEY_ALREADY_EXIST) {
        return META_KEY_ALREADY_EXIST;
    } else {
        DEBUG_STDERR(regionName, "Put failed.");
        return META_ERROR;
    }
    return ret;
}

/**
 * get_regionid_from_regionname_KVS - Helper function to lookup  a region Id
 * 	from region Name KVS tree
 * @param regionName - Region Name
 * @param regionId - Returns the Region Id
 * @return - META_NO_ERROR if key found successfully, META_KEY_DOES_NOT_EXIST if
 * 	key not found
 */
int Fam_Metadata_Service_Direct::Impl_::get_regionid_from_regionname_KVS(
    const std::string regionName, std::string &regionId) {
    int ret;

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    ret = regionNameKVS->Get(regionName.c_str(), regionName.size(), val_buf,
                             val_len);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not found");
        return ret;
    } else if (ret == META_NO_ERROR) {
        regionId.assign(val_buf, val_len);
        return ret;
    }
    return META_ERROR;
}

inline bool
Fam_Metadata_Service_Direct::Impl_::create_heap_for_dataitem_metadata_KVS(
    const uint64_t regionId, size_t heap_size) {
    ostringstream message;
    // Check if heap which will be used for storing dataitem metadata KVS
    // already present, create otherwise.
    bool isHeapCreated;
    Heap *heap = memoryManager->FindHeap((PoolId)regionId);
    if (!heap) {
        int ret = memoryManager->CreateHeap((PoolId)regionId, heap_size);
        if (ret != nvmm::NO_ERROR) {
            message << "Failed to create heap for dataitem metadata KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        isHeapCreated = true;
    } else {
        isHeapCreated = false;
        delete heap;
    }
    return isHeapCreated;
}

/**
 * metadata_insert_region - Insert the Region id key in the
 * 	region ID metadata KVS and add a region name to
 * 	region ID mapping in region name KVS
 * @param regionId - Region Id
 * @param regionName - Name of the Region
 * @param region -  Region descriptor to be added
 * @return - META_NO_ERROR if key added, META_KEY_ALREADY_EXIST id the regionID
 *   	or reginName already exist.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_insert_region(
    const uint64_t regionId, const std::string regionName,
    Fam_Region_Metadata *region) {

    ostringstream message;
    int ret;

    std::string regionKey = std::to_string(regionId);

    // Insert region name -> region id mapping in regionDataKVS
    ret = insert_in_regionname_kvs(regionName, regionKey);
    size_t tmpSize = region->size / 4;
    tmpSize = (tmpSize < MIN_HEAP_SIZE ? MIN_HEAP_SIZE : tmpSize);
    if (ret == META_NO_ERROR) {
        // Region key does not exist, create an entry

        // Create a root entry for data item tree and insert the value
        // pointer
        GlobalPtr dataitemIdRoot, dataitemNameRoot;
        if (use_meta_region) {
            dataitemIdRoot = create_metadata_kvs_tree();
            dataitemNameRoot = create_metadata_kvs_tree();
        } else {
            region->isHeapCreated =
                create_heap_for_dataitem_metadata_KVS(regionId, tmpSize);
            dataitemIdRoot =
                create_metadata_kvs_tree(tmpSize, (PoolId)regionId);
            dataitemNameRoot =
                create_metadata_kvs_tree(tmpSize, (PoolId)regionId);
        }

        // Store the root pointer in region
        region->dataItemIdRoot = dataitemIdRoot;
        region->dataItemNameRoot = dataitemNameRoot;

        // Insert the Region metadata in region ID KVS
        ret = insert_in_regionid_kvs(regionKey, region, 1);
        if (ret != META_NO_ERROR) {
            // Could not insert the region id in KVS.
            // Remove the region name from region name KVS
            regionNameKVS->Del(regionName.c_str(), regionName.size());
            if (ret == META_KEY_ALREADY_EXIST) {
                message << "Region already exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_EXIST,
                                message.str().c_str());
            } else {
                message << "Region Id insertion failed";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        OPEN_METADATA_KVS(dataitemIdRoot, tmpSize, regionId, dataitemIdKVS);
        if (dataitemIdKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            message << "Dataitem Id KVS creation failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        OPEN_METADATA_KVS(dataitemNameRoot, tmpSize, regionId, dataitemNameKVS);

        if (dataitemNameKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            message << "Dataitem name KVS creation failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // Insert the KVS pointer into map
        diKVS *kvs = new diKVS();
        kvs->diNameKVS = dataitemNameKVS;
        kvs->diIdKVS = dataitemIdKVS;
        pthread_rwlock_init(&kvs->kvsLock, NULL);
        pthread_rwlock_wrlock(&kvsMapLock);
        auto res = metadataKvsMap->insert({ regionId, kvs });
        if (!res.second) {
            pthread_rwlock_unlock(&kvsMapLock);
            pthread_rwlock_destroy(&kvs->kvsLock);
            delete dataitemNameKVS;
            delete dataitemIdKVS;
            delete kvs;
            message << "KVS pointers already exist in map";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        pthread_rwlock_unlock(&kvsMapLock);
    } else if (ret == META_KEY_ALREADY_EXIST) {
        message << "Region already exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_EXIST,
                        message.str().c_str());
    } else {
        DEBUG_STDERR(regionId, "Insert failed.");
        message << "Region metadata insertion failed";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
    }
}

/**
 * metadata_modify_region - Update the Region descriptor in the
 * 	region ID metadata KVS.
 * @param regionId - Region Id
 * @param region -  Region descriptor to be added
 * @return - META_NO_ERROR if key added, META_KEY_DOES_NOT_EXIST if regionId
 * 	key not found
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_region(
    const uint64_t regionId, Fam_Region_Metadata *region) {
    ostringstream message;

    int ret;

    Fam_Region_Metadata regNode;

    if (!metadata_find_region(regionId, regNode)) {
        // Region key does not exist
        DEBUG_STDOUT(regionId, "Region not found.");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    } else {
        // KVS found... update the value of dataitem root from the region node
        std::string regionKey = std::to_string(regionId);
        region->isHeapCreated = regNode.isHeapCreated;
        region->dataItemIdRoot = regNode.dataItemIdRoot;
        region->dataItemNameRoot = regNode.dataItemNameRoot;

        // Insert the Region metadata descriptor in the region ID KVS
        ret = insert_in_regionid_kvs(regionKey, region, 0);
        if (ret != META_NO_ERROR) {
            message << "Region metadata modification failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    }
}

/**
 * metadata_modify_region - Update the Region descriptor in the
 * 	region ID metadata KVS.
 * @param regionName - Region Id
 * @param region -  Region descriptor to be added
 * @return - META_NO_ERROR if key added, META_KEY_DOES_NOT_EXIST if regionId
 * 	key not found
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_region(
    const std::string regionName, Fam_Region_Metadata *region) {
    ostringstream message;

    int ret;

    Fam_Region_Metadata regNode;

    if (metadata_find_region(regionName, regNode)) {
        // KVS found... update the value of dataitem root from the region node

        std::string regionKey = std::to_string(regNode.regionId);
        region->isHeapCreated = regNode.isHeapCreated;
        region->dataItemIdRoot = regNode.dataItemIdRoot;
        region->dataItemNameRoot = regNode.dataItemNameRoot;

        // Insert the Region metadata descriptor in the region ID KVS
        ret = insert_in_regionid_kvs(regionKey, region, 0);
        if (ret != META_NO_ERROR) {
            message << "Region metadata modification failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        // Region key does not exist
        DEBUG_STDOUT(regionName, "Region not found.");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

inline void Fam_Metadata_Service_Direct::Impl_::destroy_dataitem_metadata_KVS(
    const uint64_t regionId, Fam_Region_Metadata regNode) {
    ostringstream message;
    // Destroying heap incase heap is created by metadata service
    // which is used for dataitem metadata KVS
    if (regNode.isHeapCreated) {
        Heap *heap = memoryManager->FindHeap((PoolId)(regionId));
        if (heap) {
            int ret = memoryManager->DestroyHeap((PoolId)(regionId));
            if (ret != nvmm::NO_ERROR) {
                message
                    << "Failed to destroy heap used for dataitem metadata KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
            delete heap;
        }
    }
}
/**
 * metadata_delete_region - delete the Region key in the region metadata KVS
 * @param regionName - Name of the Region to be deleted
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if key not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_region(
    const std::string regionName) {
    ostringstream message;

    std::string regionId;
    uint64_t regionid;
    Fam_Region_Metadata regNode;
    // Get the regionID from the region Name KVS
    int ret = get_regionid_from_regionname_KVS(regionName, regionId);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not found.");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    } else if (ret == META_NO_ERROR) {
        if (!metadata_find_region(regionName, regNode)) {
            DEBUG_STDOUT(regionName, "Region id not found.");
            message << "Region does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                            message.str().c_str());
        }

        // Delete the entry from region ID KVS
        ret = regionIdKVS->Del(regionId.c_str(), regionId.size());
        if (ret != META_NO_ERROR) {
            if (ret == META_KEY_DOES_NOT_EXIST) {
                DEBUG_STDOUT(regionName, "Region id not found.");
                message << "Region does not exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                                message.str().c_str());
            } else {
                DEBUG_STDERR(regionName, "Del failed.");
                message << "Region Id entry removal failed";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        // delete the region Name -> region Id mapping from regin name KVS
        ret = regionNameKVS->Del(regionName.c_str(), regionName.size());
        if (ret != META_NO_ERROR) {
            if (ret == META_KEY_DOES_NOT_EXIST) {
                DEBUG_STDOUT(regionName, "Region not found.");
                message << "Region does not exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                                message.str().c_str());
            } else {
                DEBUG_STDERR(regionName, "Del failed.");
                message << "Region name entry removal failed";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        pthread_rwlock_wrlock(&kvsMapLock);
        regionid = stoull(regionId);
        auto kvsObj = metadataKvsMap->find(regionid);
        if (kvsObj != metadataKvsMap->end()) {
            diKVS *kvs = kvsObj->second;
            metadataKvsMap->erase(kvsObj);
            pthread_rwlock_unlock(&kvsMapLock);
            pthread_rwlock_wrlock(&kvs->kvsLock);
            delete kvs->diIdKVS;
            delete kvs->diNameKVS;
            pthread_rwlock_unlock(&kvs->kvsLock);
            pthread_rwlock_destroy(&kvs->kvsLock);
            delete kvs;
        } else {
            pthread_rwlock_unlock(&kvsMapLock);
        }

        destroy_dataitem_metadata_KVS(regionid, regNode);
    }
}

/**
 * metadata_delete_region - delete the Region key in the region metadata KVS
 * @param regionId - Region ID key to be deleted
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if key not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_region(
    const uint64_t regionId) {

    ostringstream message;

    Fam_Region_Metadata regNode;

    int ret;
    if (metadata_find_region(regionId, regNode)) {
        // Get the region name for region metadata descriptor and
        // remove the name key from region Name KVS
        std::string regionName = regNode.name;

        ret = regionNameKVS->Del(regionName.c_str(), regionName.size());
        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionId, "Region not found");
            message << "Region does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                            message.str().c_str());
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionId, "Del failed.");
            message << "Region name entry removal failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // delete the region id for region ID KVS
        std::string regionKey = std::to_string(regionId);
        ret = regionIdKVS->Del(regionKey.c_str(), regionKey.size());
        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionId, "Region not found");
            message << "Region does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                            message.str().c_str());
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionId, "Del failed.");
            message << "Region Id entry removal failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        pthread_rwlock_wrlock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(regionId);
        if (kvsObj != metadataKvsMap->end()) {
            diKVS *kvs = kvsObj->second;
            metadataKvsMap->erase(kvsObj);
            pthread_rwlock_unlock(&kvsMapLock);
            pthread_rwlock_wrlock(&kvs->kvsLock);
            delete kvs->diIdKVS;
            delete kvs->diNameKVS;
            pthread_rwlock_unlock(&kvs->kvsLock);
            pthread_rwlock_destroy(&kvs->kvsLock);
            delete kvs;
        } else {
            pthread_rwlock_unlock(&kvsMapLock);
        }

        // Destroying heap incase heap is created by metadata service
        // which is used for dataitem metadata KVS
        destroy_dataitem_metadata_KVS(regionId, regNode);
    } else {
        DEBUG_STDOUT(regionId, "Region lookup failed.");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_insert_dataitem - Insert the dataitem id in the dataitem KVS
 * @param dataitemId- dataitem Id to be inserted
 * @param regionName - Name of the Region to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be added
 * @param  dataitemName - optional name for dataitem
 * @return - META_NO_ERROR if key added , META_KEY_DOES_NOT_EXIST if region
 *           not found, META_KEY_ALREADY_EXIST if dataitem Id entry already
 * 	     exist.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_insert_dataitem(
    const uint64_t dataitemId, std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    ostringstream message;
    int ret;

    Fam_Region_Metadata regNode;
    if (metadata_find_region(regionName.c_str(), regNode)) {
        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);
        char val_buf[max_val_len];
        size_t val_len;

        // If dataitemName is provided, create a dataitem Name -> Id mapping
        if (!dataitemName.empty()) {

            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemNameKVS->FindOrCreate(
                dataitemName.c_str(), dataitemName.size(), dataitemKey.c_str(),
                dataitemKey.size(), val_buf, val_len);

            if (ret != META_NO_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "FindOrCreate failed");
                if (ret == META_KEY_ALREADY_EXIST) {
                    message << "Dataitem already exist";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_EXIST,
                                    message.str().c_str());
                } else {
                    message
                        << "Failed to insert metadata into dataitem name KVS";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                    message.str().c_str());
                }
            }
        }

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->FindOrCreate(
            dataitemKey.c_str(), dataitemKey.size(), val_node,
            sizeof(Fam_DataItem_Metadata), val_buf, val_len);

        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "FindOrCreate failed");
            if (ret == META_KEY_ALREADY_EXIST) {
                message << "Dataitem already exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_EXIST,
                                message.str().c_str());
            } else {
                // if entry was added in dataitem name KVS, then delete it
                if (!dataitemName.empty()) {
                    int ret1 = dataitemNameKVS->Del(dataitemName.c_str(),
                                                    dataitemName.size());
                    if (ret1 != META_NO_ERROR) {
                        message
                            << "Failed to remove dataitem name entry from KVS, "
                               "while cleanup";
                        THROW_ERRNO_MSG(Metadata_Service_Exception,
                                        METADATA_ERROR, message.str().c_str());
                    }
                }
                message << "Failed to insert metadata into dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        DEBUG_STDOUT(regionName, "region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_insert_dataitem - Insert the dataitem id in the dataitem KVS
 * @param dataitemId- dataitem Id to be inserted
 * @param regionId - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be added
 * @param  dataitemName - optional name for dataitem
 * @return - META_NO_ERROR if key added , META_KEY_DOES_NOT_EXIST if region
 *           not found, META_KEY_ALREADY_EXIST if dataitem Id entry already
 * 	     exist.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    ostringstream message;
    int ret;
    Fam_Region_Metadata regNode;
    if (metadata_find_region(regionId, regNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        std::string dataitemKey = std::to_string(dataitemId);
        char val_buf[max_val_len];
        size_t val_len;

        // If dataitemName is provided, create a dataitem Name -> Id mapping
        if (!dataitemName.empty()) {

            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemNameKVS->FindOrCreate(
                dataitemName.c_str(), dataitemName.size(), dataitemKey.c_str(),
                dataitemKey.size(), val_buf, val_len);

            if (ret != META_NO_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemName, "FindOrCreate failed");
                if (ret == META_KEY_ALREADY_EXIST) {
                    message << "Dataitem already exist";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_EXIST,
                                    message.str().c_str());
                } else {
                    message
                        << "Failed to insert metadata into dataitem name KVS";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                    message.str().c_str());
                }
            }
        }
        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->FindOrCreate(
            dataitemKey.c_str(), dataitemKey.size(), val_node,
            sizeof(Fam_DataItem_Metadata), val_buf, val_len);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemKey, "FindOrCreate failed.");
            if (ret == META_KEY_ALREADY_EXIST) {
                message << "Dataitem already exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_EXIST,
                                message.str().c_str());
            } else {
                // if entry was added in dataitem name KVS, then delete it
                if (!dataitemName.empty()) {
                    int ret1 = dataitemNameKVS->Del(dataitemName.c_str(),
                                                    dataitemName.size());
                    if (ret1 != META_NO_ERROR) {
                        message
                            << "Failed to remove dataitem name entry from KVS, "
                               "while cleanup";
                        THROW_ERRNO_MSG(Metadata_Service_Exception,
                                        METADATA_ERROR, message.str().c_str());
                    }
                }
                message << "Failed to insert metadata into dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        DEBUG_STDOUT(regionId, "region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemId- dataitem Id of the dataitem descp to be modified
 * @param regionId - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *  	region not found or dataitem Id entry not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {

    ostringstream message;
    int ret;

    // Check if dataitem exists
    // if no_error, update new entry
    // if not found, return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemId, regionId, dataitemNode)) {
        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Put failed");
            message << "Failed to update dataitem metadata";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // Check if name was not assigned earlier and now name is being
        // assigned
        if ((strlen(dataitemNode.name) == 0) && (strlen(dataitem->name) > 0)) {

            std::string dataitemName = dataitem->name;
            ret =
                dataitemNameKVS->Put(dataitemName.c_str(), dataitemName.size(),
                                     dataitemKey.c_str(), dataitemKey.size());

            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "Put failed");
                message << "Failed to insert dataitem name entry";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemId- dataitem Id of the dataitem descp to be modified
 * @param regionName - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *	region not found or dataitem Id entry not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    ostringstream message;

    int ret;

    // Check if dataitem exists
    // if no_error, update the entry

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemId, regionName, dataitemNode)) {
        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        // Get the regionID from the region Name KVS
        std::string regionId;

        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            message << "Failed to get region Id from region name";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Put failed");
            message << "Failed to update dataitem metadata";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // Check if name was not assigned earlier and now name is being
        // assigned
        if ((strlen(dataitemNode.name) == 0) && (strlen(dataitem->name) > 0)) {

            if (dataitemNameKVS == nullptr) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "KVS creation failed");
                message << "Dataitem name KVS does not exist";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }

            std::string dataitemName = dataitem->name;

            ret =
                dataitemNameKVS->Put(dataitemName.c_str(), dataitemName.size(),
                                     dataitemKey.c_str(), dataitemKey.size());
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "Put failed");
                message << "Failed to insert dataitem name entry";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemName- dataitem name of the dataitem descp to be modified
 * @param regionID - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 * 	region not found or dataitem Id entry not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    ostringstream message;

    int ret;

    // Check if dataitem exists
    // if no_error, update new entry
    // if not found, return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemName, regionId, dataitemNode)) {
        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey;

        char val_buf[max_val_len];
        size_t val_len;

        ResetBuf(val_buf, val_len, max_val_len);

        // Get the id for the dataitem name from dataitem Name KVS
        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {
            dataitemKey.assign(val_buf, val_len);

            ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                     val_node, sizeof(Fam_DataItem_Metadata));
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemName, "Put failed");
                message << "Failed to update dtaitem metadata";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemName- dataitem name of the dataitem descp to be modified
 * @param regionName - Region Name to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *           region not found or dataitem Id entry not found.
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    ostringstream message;

    int ret;

    // Check if dataitem exists
    // if no_error, create new entry
    // return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemName, regionName, dataitemNode)) {
        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            message << "Failed to get region Id from region name";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey;

        char val_buf[max_val_len];
        size_t val_len;

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {
            dataitemKey.assign(val_buf, val_len);

            ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                     val_node, sizeof(Fam_DataItem_Metadata));
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemName, "Put failed.");
                message << "Failed to update dtaitem metadata";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemId - dataitem Id in the region to be deleted
 * @param regionName - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {
    ostringstream message;

    int ret;

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemId, regionName, dataitemNode)) {

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            message << "Failed to get region Id from region name";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Del failed.");
            message << "Failed to remove metadata from dataitem Id KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemName = dataitemNode.name;
        if (!dataitemName.empty()) {
            ret =
                dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "Del failed.");
                message << "Failed to remove dataitem name entry from KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemId - dataitem Id in the region to be deleted
 * @param regionId - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_dataitem(
    const uint64_t dataitemId, const uint64_t regionId) {

    ostringstream message;
    int ret;

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemId, regionId, dataitemNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Del failed.");
            message << "Failed to remove metadata from dataitem Id KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemName = dataitemNode.name;
        if (!dataitemName.empty()) {
            ret =
                dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemId, "Del failed.");
                message << "Failed to remove dataitem name entry from KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemName - dataitem Name in the region to be deleted
 * @param regionId - Id of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {

    ostringstream message;
    int ret;

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemName, regionId, dataitemNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey;

        char val_buf[max_val_len];
        size_t val_len;

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {
            dataitemKey.assign(val_buf, val_len);
            ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemName, "Del failed.");
                message << "Failed to remove metadata from dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemName, "Del failed.");
            message << "Failed to remove dataitem name entry from KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemName - dataitem Name in the region to be deleted
 * @param regionName - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if region or daatitem key does not exists
 */
void Fam_Metadata_Service_Direct::Impl_::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {

    ostringstream message;
    int ret;

    Fam_DataItem_Metadata dataitemNode;
    if (metadata_find_dataitem(dataitemName, regionName, dataitemNode)) {
        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            message << "Failed to get region Id from region name";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey;

        char val_buf[max_val_len];
        size_t val_len;

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {
            dataitemKey.assign(val_buf, val_len);
            ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
            if (ret == META_ERROR) {
                release_kvs_lock(kvsLock);
                DEBUG_STDERR(dataitemName, "Del failed.");
                message << "Failed to remove metadata from dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemName, "Del failed.");
            message << "Failed to remove dataitem name entry from KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        release_kvs_lock(kvsLock);
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_find_dataitem - Lookup a dataitem name in metadata region KVS
 * @param dataitemId - dataitem Id
 * @param regionId - Region Id to which dataitem belongs.
 * @param &dataitem - returns Descriptor to the cwdataitem region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if key not
 *found
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    ostringstream message;

    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    if (metadata_find_region(regionId, regNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        release_kvs_lock(kvsLock);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));
            return true;
        } else if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        } else {
            message << "Failed to find dataitem";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionId, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_find_dataitem - Lookup a dataitem name in metadata region KVS
 * @param dataitemId - dataitem Id
 * @param regionName - Name of the Region to which dataitem belongs.
 * @param &dataitem - returns Descriptor to the cwdataitem region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if key not
 *found
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    ostringstream message;
    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    if (metadata_find_region(regionName, regNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        release_kvs_lock(kvsLock);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));
            return true;
        } else if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        } else {
            message << "Failed to find dataitem";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_find_dataitem - Lookup a dataitem name in metadata region KVS
 * @param dataitemName - Name of the  dataitem
 * @param regionId - Id of the Region to which dataitem belongs.
 * @param &dataitem - returns Descriptor to the cwdataitem region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if key not
 *found
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    ostringstream message;

    int ret;
    Fam_Region_Metadata regNode;

    if (metadata_find_region(regionId, regNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey;
        char val_buf[max_val_len];
        size_t val_len;
        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {
            dataitemKey.assign(val_buf, val_len);
            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                     val_buf, val_len);
            release_kvs_lock(kvsLock);
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));
                return true;
            } else if (ret == META_KEY_DOES_NOT_EXIST) {
                DEBUG_STDERR(dataitemId, "Get failed.");
                return false;
            } else {
                message << "Failed to find dataitem";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        } else if (ret == META_KEY_DOES_NOT_EXIST) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        } else {
            release_kvs_lock(kvsLock);
            message << "Failed to find dataitem";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_find_dataitem - Lookup a dataitem name in metadata region KVS
 * @param dataitemName - Name of the  dataitem
 * @param regionName - Name of the Region to which dataitem belongs.
 * @param &dataitem - returns Descriptor to the cwdataitem region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if key not
 *found
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    ostringstream message;
    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    if (metadata_find_region(regionName, regNode)) {
        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        pthread_rwlock_t *kvsLock;
        ret = get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS,
                               &kvsLock);
        if (ret != META_NO_ERROR) {
            release_kvs_lock(kvsLock);
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        std::string dataitemKey;

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_NO_ERROR) {

            dataitemKey.assign(val_buf, val_len);

            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                     val_buf, val_len);
            release_kvs_lock(kvsLock);
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));
                return true;
            } else if (ret == META_KEY_DOES_NOT_EXIST) {
                DEBUG_STDERR(dataitemId, "Get failed.");
                return false;
            } else {
                message << "Failed to find dataitem";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        } else if (ret == META_KEY_DOES_NOT_EXIST) {
            release_kvs_lock(kvsLock);
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        } else {
            release_kvs_lock(kvsLock);
            message << "Failed to find dataitem";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
}

/**
 * metadata_check_permissions - Check if uid/gid has
 *   	the required permission.
 * @params dataitem - Dataitem descriptor
 * @params op - operation for which permission check is being made
 * @params uid - user id
 * @params gid - group id
 * Returns true if uid/gid has required permission, else false
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {

    bool write = false, read = false, exec = false;
    if (dataitem->uid == uid) {
        if (op & META_REGION_ITEM_WRITE) {
            write = write || (dataitem->perm & S_IWUSR) ? true : false;
        }

        if (op & META_REGION_ITEM_READ) {
            read = read || (dataitem->perm & S_IRUSR) ? true : false;
        }

        if (op & META_REGION_ITEM_EXEC) {
            exec = exec || (dataitem->perm & S_IXUSR) ? true : false;
        }
    }

    if (dataitem->gid == gid) {
        if (op & META_REGION_ITEM_WRITE) {
            write = write || (dataitem->perm & S_IWGRP) ? true : false;
        }

        if (op & META_REGION_ITEM_READ) {
            read = read || (dataitem->perm & S_IRGRP) ? true : false;
        }

        if (op & META_REGION_ITEM_EXEC) {
            exec = exec || (dataitem->perm & S_IXGRP) ? true : false;
        }
    }
    if (op & META_REGION_ITEM_WRITE) {
        write = write || (dataitem->perm & S_IWOTH) ? true : false;
    }

    if (op & META_REGION_ITEM_READ) {
        read = read || (dataitem->perm & S_IROTH) ? true : false;
    }

    if (op & META_REGION_ITEM_EXEC) {
        exec = exec || (dataitem->perm & S_IXOTH) ? true : false;
    }

    if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_READ) &&
        (op & META_REGION_ITEM_EXEC)) {
        if (!(write && read && exec))
            return false;
    } else if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_READ)) {
        if (!(write && read))
            return false;
    } else if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_EXEC)) {
        if (!(write && exec))
            return false;
    } else if ((op & META_REGION_ITEM_READ) && (op & META_REGION_ITEM_EXEC)) {
        if (!(read && exec))
            return false;
    } else if (op & META_REGION_ITEM_WRITE) {
        if (!write)
            return false;
    } else if (op & META_REGION_ITEM_READ) {
        if (!read)
            return false;
    } else if (op & META_REGION_ITEM_EXEC) {
        if (!exec)
            return false;
    }

    return true;
}

/**
 * metadata_check_permissions - Check if uid/gid has
 *   	the required permission.
 * @params dataitem - Region descriptor
 * @params op - operation for which permission check is being made
 * @params uid - user id
 * @params gid - group id
 * Returns true if uid/gid has required permission, else false
 *
 */
bool Fam_Metadata_Service_Direct::Impl_::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {

    bool write = false, read = false, exec = false;
    if (region->uid == uid) {
        if (op & META_REGION_ITEM_WRITE) {
            write = write || (region->perm & S_IWUSR) ? true : false;
        }

        if (op & META_REGION_ITEM_READ) {
            read = read || (region->perm & S_IRUSR) ? true : false;
        }

        if (op & META_REGION_ITEM_EXEC) {
            exec = exec || (region->perm & S_IXUSR) ? true : false;
        }
    }

    if (region->gid == gid) {
        if (op & META_REGION_ITEM_WRITE) {
            write = write || (region->perm & S_IWGRP) ? true : false;
        }

        if (op & META_REGION_ITEM_READ) {
            read = read || (region->perm & S_IRGRP) ? true : false;
        }

        if (op & META_REGION_ITEM_EXEC) {
            exec = exec || (region->perm & S_IXGRP) ? true : false;
        }
    }
    if (op & META_REGION_ITEM_WRITE) {
        write = write || (region->perm & S_IWOTH) ? true : false;
    }

    if (op & META_REGION_ITEM_READ) {
        read = read || (region->perm & S_IROTH) ? true : false;
    }

    if (op & META_REGION_ITEM_EXEC) {
        exec = exec || (region->perm & S_IXOTH) ? true : false;
    }

    if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_READ) &&
        (op & META_REGION_ITEM_EXEC)) {
        if (!(write && read && exec))
            return false;
    } else if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_READ)) {
        if (!(write && read))
            return false;
    } else if ((op & META_REGION_ITEM_WRITE) && (op & META_REGION_ITEM_EXEC)) {
        if (!(write && exec))
            return false;
    } else if ((op & META_REGION_ITEM_READ) && (op & META_REGION_ITEM_EXEC)) {
        if (!(read && exec))
            return false;
    } else if (op & META_REGION_ITEM_WRITE) {
        if (!write)
            return false;
    } else if (op & META_REGION_ITEM_READ) {
        if (!read)
            return false;
    } else if (op & META_REGION_ITEM_EXEC) {
        if (!exec)
            return false;
    }

    return true;
}

size_t Fam_Metadata_Service_Direct::Impl_::metadata_maxkeylen() {
    return regionNameKVS->MaxKeyLen();
}

void Fam_Metadata_Service_Direct::Impl_::metadata_update_memoryserver(
    int nmemServersPersistent, std::vector<uint64_t> memsrv_persistent_id_list,
    int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list) {
    pthread_mutex_lock(&memserverListLock);
    if ((memoryServerPersistentList.size() != 0) ||
        memoryServerVolatileList.size() != 0) {
        pthread_mutex_unlock(&memserverListLock);
        return;
    }
    for (int i = 0; i < nmemServersPersistent; i++) {
        memoryServerPersistentList.push_back((int)memsrv_persistent_id_list[i]);
    }
    for (int i = 0; i < nmemServersVolatile; i++) {
        memoryServerVolatileList.push_back((int)memsrv_volatile_id_list[i]);
    }
    pthread_mutex_unlock(&memserverListLock);
}

int Fam_Metadata_Service_Direct::Impl_::get_regionid_from_bitmap(
    uint64_t *regionID) {
    // Find the first free bit after 10th bit in poolId bitmap.
    // First 10 nvmm ID will be reserved.
    *regionID =
        bitmap_find_and_reserve(bmap, 0, (uint64_t)MEMSERVER_REGIONID_START);
    if (*regionID == (uint64_t)BITMAP_NOTFOUND)
        return META_NO_FREE_REGIONID;

    return META_NO_ERROR;
}

void
Fam_Metadata_Service_Direct::Impl_::metadata_reset_bitmap(uint64_t regionID) {
    bitmap_reset(bmap, regionID);
}

/**
 * metadata_validate_and_create_region - Check if region name exists and find
 *     regionid and memoryservers.
 * @params regionname - Region name to create
 * @params size - Size of the region
 * @params regionid - Region id of the region
 * @params memory_server_list - List of memory servers in which this region
 * should be created.
 * @params userpolicy - User policy recommendation
 *
 */

void Fam_Metadata_Service_Direct::Impl_::metadata_validate_and_create_region(
    const std::string regionname, size_t size, uint64_t *regionid,
    Fam_Region_Attributes *regionAttributes, std::list<int> *memory_server_list,
    int user_policy = 0) {
    ostringstream message;
    Fam_Region_Metadata region;
    //
    // Check if name length is within limit
    if (regionname.size() > metadata_maxkeylen()) {
        message << "Create region error : Region name is too long.";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NAME_TOO_LONG,
                        message.str().c_str());
    }

    // Check if region with that name exist
    if (metadata_find_region(regionname, region)) {

        message << "Create region error : Region name already exists.";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_EXIST,
                        message.str().c_str());
    }

    // Call get_regionid_from_bitmap to get regionID
    int ret = get_regionid_from_bitmap(regionid);

    if (ret == META_NO_FREE_REGIONID) {
        message << "Create region error : No free RegionID.";
        THROW_ERRNO_MSG(Metadata_Service_Exception, NO_FREE_POOLID,
                        message.str().c_str());
    }
    // Call find_memory_server for the size asked for and for user policy
    *memory_server_list = find_memory_server_list(
        regionname, size, regionAttributes->memoryType, user_policy);
    if ((*memory_server_list).size() == 0) {
        message << "Create region error : Requested Memory type not available.";
        THROW_ERRNO_MSG(Metadata_Service_Exception,
                        REQUESTED_MEMORY_TYPE_NOT_AVAILABLE,
                        message.str().c_str());
    }
}

/**
 * metadata_validate_and_destroy_region- Remove the region from metadata
 * @params regionname - Region id to destroy
 * @params uid - User id of the user
 * @params gid - gid of the user
 * @params memory_server_list - List of memory server from which the region
 * should be removed.
 *
 */

void Fam_Metadata_Service_Direct::Impl_::metadata_validate_and_destroy_region(
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    std::list<int> *memory_server_list) {
    ostringstream message;
    // Check if region exists
    // Check if the region exist, if not return error
    Fam_Region_Metadata region;
    bool ret = metadata_find_region(regionId, region);
    if (ret == 0) {
        message << "Destroy Region error : Region not found";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Destroy Region error : Insufficient Permissions";
            THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }

    // Return memory server list to user
    *memory_server_list = get_memory_server_list(regionId);
    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.

    try {
        metadata_delete_region(regionId);
    }
    catch (Fam_Exception &e) {
        message
            << "Destroy Region error : couldnt remove region with region ID: "
            << regionId << endl;
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_REMOVED,
                        message.str().c_str());
    }
}

void
Fam_Metadata_Service_Direct::Impl_::metadata_validate_and_allocate_dataitem(
    const std::string dataitemName, const uint64_t regionId, uint32_t uid,
    uint32_t gid, uint64_t *memoryServerId) {
    ostringstream message;
    bool ret;
    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (dataitemName.size() > metadata_maxkeylen()) {
        message << "Allocate Dataitem error : Dataitem name is too long.";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NAME_TOO_LONG,
                        message.str().c_str());
    }

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    ret = metadata_find_region(regionId, region);
    if (ret == 0) {
        message << "Allocate Dataitem error : Specified Region not found.";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to create dataitem in that region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Allocate Dataitem error : Insufficient Permissions";
            THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }

    // Check with metadata service if data item with the requested name
    // already exists.If exists return error.
    Fam_DataItem_Metadata dataitem;

    if (dataitemName != "") {
        ret = metadata_find_dataitem(dataitemName, regionId, dataitem);

        if (ret == 1) {
            message << "Allocate Dataitem error : Dataitem already exists";
            THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_EXIST,
                            message.str().c_str());
        }
    }
    uint64_t id =  0;
    if (!dataitemName.empty()) {
	id = (std::hash<std::string>{}(dataitemName) % region.used_memsrv_cnt);
    } else {
	id = rand() % region.used_memsrv_cnt;
    }
	*memoryServerId = region.memServerIds[id];

}

void
Fam_Metadata_Service_Direct::Impl_::metadata_validate_and_deallocate_dataitem(
    const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
    uint32_t gid) {
    ostringstream message;
    // Check with metadata service if data item with the requested name
    // is already exist, if not return error
    Fam_DataItem_Metadata dataitem;
    bool ret = metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret == 0) {
        message << "Deallocate Dataitem error : Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != dataitem.uid) {
        bool isPermitted = metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            message << "Deallocate dataitem error : Insufficient Permissions";
            THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                            message.str().c_str());
        }
    }
    // Remove data item from metadata service
    try {
        metadata_delete_dataitem(dataitemId, regionId);
    }
    catch (Fam_Exception &e) {
        message << "Deallocate dataitem error : Couldnt delete dataitem";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_REMOVED,
                        message.str().c_str());
    }
}

void
Fam_Metadata_Service_Direct::Impl_::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    ostringstream message;
    // Check with metadata service if the region exist, if not return error
    bool ret = metadata_find_region(regionId, region);
    if (ret == 0) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
    if (((op & META_OWNER_ALLOW) > 0) && (uid == region.uid)) {
        return;
    }

    bool isPermitted = metadata_check_permissions(&region, op, uid, gid);
    if (!isPermitted) {
        message << "Insufficient Permission";
        THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                        message.str().c_str());
    }
}

void Fam_Metadata_Service_Direct::Impl_::
    metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const uint64_t dataitemId,
        const uint64_t regionId, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem) {
    ostringstream message;

    if (!metadata_find_dataitem(dataitemId, regionId, dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (((op & META_OWNER_ALLOW) > 0) && (uid == dataitem.uid)) {
        return;
    }
    bool isPermitted = metadata_check_permissions(&dataitem, op, uid, gid);
    if (!isPermitted) {
        message << "Insufficient Permission";
        THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                        message.str().c_str());
    }
}

void
Fam_Metadata_Service_Direct::Impl_::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const std::string regionName, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    ostringstream message;
    // Check with metadata service if the region exist, if not return error
    bool ret = metadata_find_region(regionName, region);
    if (ret == 0) {
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
    if (((op & META_OWNER_ALLOW) > 0) && (uid == region.uid)) {
        return;
    }
    bool isPermitted = metadata_check_permissions(&region, op, uid, gid);
    if (!isPermitted) {
        message << "Insufficient Permission";
        THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                        message.str().c_str());
    }
}

void Fam_Metadata_Service_Direct::Impl_::
    metadata_find_dataitem_and_check_permissions(
        metadata_region_item_op_t op, const std::string dataitemName,
        const std::string regionName, uint32_t uid, uint32_t gid,
        Fam_DataItem_Metadata &dataitem) {
    ostringstream message;

    if (!metadata_find_dataitem(dataitemName, regionName, dataitem)) {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, DATAITEM_NOT_FOUND,
                        message.str().c_str());
    }
    if (((op & META_OWNER_ALLOW) > 0) && (uid == dataitem.uid)) {
        return;
    }
    bool isPermitted = metadata_check_permissions(&dataitem, op, uid, gid);
    if (!isPermitted) {
        message << "Insufficient Permission";
        THROW_ERRNO_MSG(Metadata_Service_Exception, NO_PERMISSION,
                        message.str().c_str());
    }
}

std::list<int> Fam_Metadata_Service_Direct::Impl_::find_memory_server_list(
    const std::string regionname, size_t size, Fam_Memory_Type memoryType,
    int user_policy) {
    std::list<int> memsrv_list;

    std::vector<uint64_t> memoryServerList;
    int memoryServerCount;
    ostringstream message;
    std::uint64_t hashVal = std::hash<std::string> {}
    (regionname);
    unsigned int i = 0;
    uint64_t aligned_size = align_to_address(size, 64);
    size = (aligned_size > size ? aligned_size : size);
    if (memoryType == PERSISTENT) {
        memoryServerList = memoryServerPersistentList;
        memoryServerCount = (int)memoryServerPersistentList.size();
    } else {
        memoryServerList = memoryServerVolatileList;
        memoryServerCount = (int)memoryServerVolatileList.size();
    }

    unsigned int id = (int)(hashVal % memoryServerCount);
    if (memoryServerCount == 0) {
        message << "Create region error : Requested Memory type not available.";
        THROW_ERRNO_MSG(Metadata_Service_Exception,
                        REQUESTED_MEMORY_TYPE_NOT_AVAILABLE,
                        message.str().c_str());
    }
    if (enable_region_spanning == 1) {
        if (size <= region_span_size_per_memoryserver) {
            // Size is smaller than region_span_size_per_memoryserver and hence
            // using single memory server.
            memsrv_list.push_back((int)memoryServerList[id]);
        } else {
            if ((size /
                     (region_span_size_per_memoryserver * memoryServerCount) <
                 1)) {
                unsigned int numServers =
                    ((unsigned int)(size / region_span_size_per_memoryserver)) +
                    ((size % region_span_size_per_memoryserver) == 0 ? 0 : 1);
                while (i < numServers) {
                    memsrv_list.push_back(
                        (int)memoryServerList[id % memoryServerCount]);
                    i++;
                    id++;
                }
            } else {
                while (i < (unsigned int)memoryServerCount) {
                    memsrv_list.push_back(
                        (int)memoryServerList[id % memoryServerCount]);
                    i++;
                    id++;
                }
            }
        }
    } else {
        memsrv_list.push_back((int)memoryServerList[id]);
    }
    return memsrv_list;
}

std::list<int>
Fam_Metadata_Service_Direct::Impl_::get_memory_server_list(uint64_t regionId) {
    ostringstream message;
    Fam_Region_Metadata region;
    std::list<int> memsrv_list;
    if (!metadata_find_region(regionId, region)) {
        message << "Region not found";
        THROW_ERRNO_MSG(Metadata_Service_Exception, REGION_NOT_FOUND,
                        message.str().c_str());
    }
    for (uint64_t i = 0; i < region.used_memsrv_cnt; i++) {
        memsrv_list.push_back((int)region.memServerIds[i]);
    }
    return memsrv_list;
}

Fam_Metadata_Service_Direct::Fam_Metadata_Service_Direct(bool use_meta_reg) {
    // Look for options information from config file.
    // Use config file options only if NULL is passed.
    std::string config_file_path;
    configFileParams config_options;
    bool enable_region_spanning;
    size_t region_span_size_per_memoryserver;

    // Check for config file in or in path mentioned
    // by OPENFAM_ROOT environment variable or in /opt/OpenFAM.
    try {
        config_file_path = find_config_file(strdup("fam_metadata_config.yaml"));
    }
    catch (Fam_InvalidOption_Exception &e) {
        // If the config_file is not present, then ignore the exception.
    }
    // Get the configuration info from the configruation file.
    if (!config_file_path.empty()) {
        config_options = get_config_info(config_file_path);
    }
    if ((config_options["enable_region_spanning"]) == "true") {
        enable_region_spanning = 1;
    } else {
        enable_region_spanning = 0;
    }
    region_span_size_per_memoryserver =
        atoi((const char *)(config_options["region_span_size_per_memoryserver"]
                                .c_str()));

    Start(use_meta_reg, enable_region_spanning,
          region_span_size_per_memoryserver);
}

Fam_Metadata_Service_Direct::~Fam_Metadata_Service_Direct() { Stop(); }

void Fam_Metadata_Service_Direct::Stop() {
    int ret = pimpl_->Final();
    assert(ret == META_NO_ERROR);
    if (pimpl_)
        delete pimpl_;
}

void
Fam_Metadata_Service_Direct::Start(bool use_meta_reg,
                                   bool enable_region_spanning,
                                   size_t region_span_size_per_memoryserver) {

    MEMSERVER_PROFILE_INIT(METADATA_DIRECT)
    MEMSERVER_PROFILE_START_TIME(METADATA_DIRECT)
    StartNVMM();
    pimpl_ = new Impl_;
    assert(pimpl_);
    int ret = pimpl_->Init(use_meta_reg, enable_region_spanning,
                           region_span_size_per_memoryserver);
    assert(ret == META_NO_ERROR);
}

void Fam_Metadata_Service_Direct::reset_profile() {
    MEMSERVER_PROFILE_INIT(METADATA_DIRECT)
    MEMSERVER_PROFILE_START_TIME(METADATA_DIRECT)
}

void Fam_Metadata_Service_Direct::dump_profile() {
    METADATA_DIRECT_PROFILE_DUMP();
}

void Fam_Metadata_Service_Direct::metadata_insert_region(
    const uint64_t regionID, const std::string regionName,
    Fam_Region_Metadata *region) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_insert_region(regionID, regionName, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_insert_region);
}

void
Fam_Metadata_Service_Direct::metadata_delete_region(const uint64_t regionID) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_region(regionID);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_region);
}

void Fam_Metadata_Service_Direct::metadata_delete_region(
    const std::string regionName) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_region(regionName);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_region);
}

bool
Fam_Metadata_Service_Direct::metadata_find_region(const std::string regionName,
                                                  Fam_Region_Metadata &region) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_region(regionName, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_region);
    return ret;
}

bool
Fam_Metadata_Service_Direct::metadata_find_region(const uint64_t regionId,
                                                  Fam_Region_Metadata &region) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_region(regionId, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_region);
    return ret;
}

void Fam_Metadata_Service_Direct::metadata_modify_region(
    const uint64_t regionID, Fam_Region_Metadata *region) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_region(regionID, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_region);
}
void Fam_Metadata_Service_Direct::metadata_modify_region(
    const std::string regionName, Fam_Region_Metadata *region) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_region(regionName, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_region);
}

void Fam_Metadata_Service_Direct::metadata_insert_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_insert_dataitem(dataitemId, regionName, dataitem,
                                     dataitemName);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_insert_dataitem(dataitemId, regionId, dataitem,
                                     dataitemName);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_insert_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_dataitem(dataitemId, regionName);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_dataitem);
}

void
Fam_Metadata_Service_Direct::metadata_delete_dataitem(const uint64_t dataitemId,
                                                      const uint64_t regionId) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_dataitem(dataitemId, regionId);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_dataitem(dataitemName, regionName);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_delete_dataitem(dataitemName, regionId);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_delete_dataitem);
}

bool Fam_Metadata_Service_Direct::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemId, regionId, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_dataitem);
    return ret;
}

bool Fam_Metadata_Service_Direct::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemId, regionName, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_dataitem);
    return ret;
}

bool Fam_Metadata_Service_Direct::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {

    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemName, regionId, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_dataitem);
    return ret;
}

bool Fam_Metadata_Service_Direct::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemName, regionName, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_dataitem);
    return ret;
}

void Fam_Metadata_Service_Direct::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_dataitem(dataitemId, regionId, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_dataitem(dataitemId, regionName, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_dataitem(dataitemName, regionId, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_modify_dataitem(dataitemName, regionName, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_modify_dataitem);
}

bool Fam_Metadata_Service_Direct::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_check_permissions(dataitem, op, uid, gid);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_check_permissions);
    return ret;
}

bool Fam_Metadata_Service_Direct::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint32_t uid,
    uint32_t gid) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_check_permissions(region, op, uid, gid);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_check_permissions);
    return ret;
}

size_t Fam_Metadata_Service_Direct::metadata_maxkeylen() {

    size_t ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_maxkeylen();
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_maxkeylen);
    return ret;
}

/*
 * get_config_info - Obtain the required information from
 * fam_pe_config file. On Success, returns a map that has options updated from
 * from configuration file. Set default values if not found in config file.
 */
configFileParams
Fam_Metadata_Service_Direct::get_config_info(std::string filename) {
    configFileParams options;
    config_info *info = NULL;
    if (filename.find("fam_metadata_config.yaml") != std::string::npos) {
        info = new yaml_config_info(filename);
        try {
            options["metadata_manager"] = (char *)strdup(
                (info->get_key_value("metadata_manager")).c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["metadata_manager"] = (char *)strdup("radixtree");
        }

        try {
            options["metadata_path"] =
                (char *)strdup((info->get_key_value("metadata_path")).c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["metadata_path"] = (char *)strdup("/dev/shm");
        }
        try {
            options["enable_region_spanning"] = (char *)strdup(
                (info->get_key_value("enable_region_spanning")).c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["enable_region_spanning"] = (char *)strdup("true");
        }
        try {
            options["region_span_size_per_memoryserver"] = (char *)strdup(
                (info->get_key_value("region_span_size_per_memoryserver"))
                    .c_str());
        }
        catch (Fam_InvalidOption_Exception e) {
            // If parameter is not present, then set the default.
            options["region_span_size_per_memoryserver"] =
                (char *)strdup("1073741824");
        }
    }
    return options;
}
void Fam_Metadata_Service_Direct::metadata_update_memoryserver(
    int nmemServersPersistent, std::vector<uint64_t> memsrv_persistent_id_list,
    int nmemServersVolatile, std::vector<uint64_t> memsrv_volatile_id_list) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_update_memoryserver(
        nmemServersPersistent, memsrv_persistent_id_list, nmemServersVolatile,
        memsrv_volatile_id_list);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_update_memoryserver);
}

void Fam_Metadata_Service_Direct::metadata_reset_bitmap(uint64_t regionId) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_reset_bitmap(regionId);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_reset_bitmap);
}

void Fam_Metadata_Service_Direct::metadata_validate_and_create_region(
    string regionname, size_t size, uint64_t *regionid,
    Fam_Region_Attributes *regionAttributes, std::list<int> *memory_server_list,
    int user_policy) {

    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_validate_and_create_region(
        regionname, size, regionid, regionAttributes, memory_server_list,
        user_policy);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_validate_and_create_region);
}

void Fam_Metadata_Service_Direct::metadata_validate_and_destroy_region(
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    std::list<int> *memory_server_list) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_validate_and_destroy_region(regionId, uid, gid,
                                                 memory_server_list);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_validate_and_destroy_region);
}

void Fam_Metadata_Service_Direct::metadata_validate_and_allocate_dataitem(
    const std::string dataitemName, const uint64_t regionId, uint32_t uid,
    uint32_t gid, uint64_t *memoryServerId) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_validate_and_allocate_dataitem(dataitemName, regionId, uid,
                                                    gid, memoryServerId);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_validate_and_allocate_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_validate_and_deallocate_dataitem(
    const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
    uint32_t gid) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_validate_and_deallocate_dataitem(regionId, dataitemId, uid,
                                                      gid);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_validate_and_deallocate_dataitem);
}

void Fam_Metadata_Service_Direct::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const uint64_t regionId, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_find_region_and_check_permissions(op, regionId, uid, gid,
                                                       region);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Direct::metadata_find_dataitem_and_check_permissions(
    metadata_region_item_op_t op, const uint64_t dataitemId,
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    Fam_DataItem_Metadata &dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_find_dataitem_and_check_permissions(
        op, dataitemId, regionId, uid, gid, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_find_dataitem_and_check_permissions);
}

void Fam_Metadata_Service_Direct::metadata_find_region_and_check_permissions(
    metadata_region_item_op_t op, const std::string regionName, uint32_t uid,
    uint32_t gid, Fam_Region_Metadata &region) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_find_region_and_check_permissions(op, regionName, uid, gid,
                                                       region);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_find_region_and_check_permissions);
}

void Fam_Metadata_Service_Direct::metadata_find_dataitem_and_check_permissions(
    metadata_region_item_op_t op, const std::string dataitemName,
    const std::string regionName, uint32_t uid, uint32_t gid,
    Fam_DataItem_Metadata &dataitem) {
    METADATA_DIRECT_PROFILE_START_OPS()
    pimpl_->metadata_find_dataitem_and_check_permissions(
        op, dataitemName, regionName, uid, gid, dataitem);
    METADATA_DIRECT_PROFILE_END_OPS(
        direct_metadata_find_dataitem_and_check_permissions);
}

std::list<int>
Fam_Metadata_Service_Direct::get_memory_server_list(uint64_t regionId) {
    std::list<int> memServerList;
    METADATA_DIRECT_PROFILE_START_OPS()
    memServerList = pimpl_->get_memory_server_list(regionId);
    METADATA_DIRECT_PROFILE_END_OPS(direct_get_memory_server_list);
    return memServerList;
}

inline uint64_t
Fam_Metadata_Service_Direct::Impl_::align_to_address(uint64_t size,
                                                     int multiple) {
    assert(multiple && ((multiple & (multiple - 1)) == 0));
    return (size + multiple - 1) & -multiple;
}

} // end namespace metadata

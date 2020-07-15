/*
 *   fam_metadata_manager.cpp
 *   Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All
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

#include "fam_metadata_manager.h"

#include <boost/atomic.hpp>

#include <atomic>
#include <chrono>
#include <iomanip>
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

using namespace famradixtree;
using namespace nvmm;
using namespace std;
using namespace chrono;

namespace metadata {
MEMSERVER_PROFILE_START(METADATA)
#ifdef MEMSERVER_PROFILE
#define METADATA_PROFILE_START_OPS()                                           \
    {                                                                          \
        Profile_Time start = METADATA_get_time();

#define METADATA_PROFILE_END_OPS(apiIdx)                                       \
    Profile_Time end = METADATA_get_time();                                    \
    Profile_Time total = METADATA_time_diff_nanoseconds(start, end);           \
    MEMSERVER_PROFILE_ADD_TO_TOTAL_OPS(METADATA, prof_##apiIdx, total)         \
    }
#define METADATA_PROFILE_DUMP() metadata_profile_dump()
#else
#define METADATA_PROFILE_START_OPS()
#define METADATA_PROFILE_END_OPS(apiIdx)
#define METADATA_PROFILE_DUMP()
#endif

void metadata_profile_dump(){MEMSERVER_PROFILE_END(METADATA)
                                 MEMSERVER_DUMP_PROFILE_BANNER(METADATA)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA, name, prof_##name)
#include "metadata/metadata_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name) MEMSERVER_PROFILE_TOTAL(METADATA, prof_##name)
#include "metadata/metadata_counters.tbl"
                                     MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA)}

KeyValueStore::IndexType const KVSTYPE = KeyValueStore::RADIX_TREE;

size_t const max_val_len = 4096;

inline void ResetBuf(char *buf, size_t &len, size_t const max_len) {
    memset(buf, 0, max_len);
    len = max_len;
}

/*
 * Internal implementation of FAM_Metadata_Manager
 */
class FAM_Metadata_Manager::Impl_ {
  public:
    Impl_() {}

    ~Impl_() {}

    int Init(bool use_meta_reg);

    int Final();

    int metadata_insert_region(const uint64_t regionId,
                               const std::string regionName,
                               Fam_Region_Metadata *region);

    int metadata_delete_region(const uint64_t regionId);

    int metadata_delete_region(const std::string regionName);

    int metadata_find_region(const uint64_t regionId,
                             Fam_Region_Metadata &region);

    int metadata_find_region(const std::string regionName,
                             Fam_Region_Metadata &region);

    int metadata_modify_region(const uint64_t regionID,
                               Fam_Region_Metadata *region);

    int metadata_modify_region(const std::string regionName,
                               Fam_Region_Metadata *region);

    int metadata_insert_dataitem(const uint64_t dataitemId,
                                 const uint64_t regionId,
                                 Fam_DataItem_Metadata *dataitem,
                                 std::string dataitemName = "");

    int metadata_insert_dataitem(const uint64_t dataitemId,
                                 const std::string regionName,
                                 Fam_DataItem_Metadata *dataitem,
                                 std::string dataitemName = "");

    int metadata_delete_dataitem(const uint64_t dataitemId,
                                 const uint64_t regionId);

    int metadata_delete_dataitem(const uint64_t dataitemId,
                                 const std::string regionName);

    int metadata_delete_dataitem(const std::string dataitemName,
                                 const uint64_t regionId);

    int metadata_delete_dataitem(const std::string dataitemName,
                                 const std::string regionName);

    int metadata_find_dataitem(const uint64_t dataitemId,
                               const uint64_t regionId,
                               Fam_DataItem_Metadata &dataitem);

    int metadata_find_dataitem(const uint64_t dataitemId,
                               const std::string regionName,
                               Fam_DataItem_Metadata &dataitem);

    int metadata_find_dataitem(const std::string dataitemName,
                               const uint64_t regionId,
                               Fam_DataItem_Metadata &dataitem);

    int metadata_find_dataitem(const std::string dataitemName,
                               const std::string regionName,
                               Fam_DataItem_Metadata &dataitem);

    int metadata_modify_dataitem(const uint64_t dataitemId,
                                 const uint64_t regionId,
                                 Fam_DataItem_Metadata *dataitem);

    int metadata_modify_dataitem(const uint64_t dataitemId,
                                 const std::string regionName,
                                 Fam_DataItem_Metadata *dataitem);

    int metadata_modify_dataitem(const std::string dataitemName,
                                 const uint64_t regionId,
                                 Fam_DataItem_Metadata *dataitem);

    int metadata_modify_dataitem(const std::string dataitemName,
                                 const std::string regionName,
                                 Fam_DataItem_Metadata *dataitem);

    bool metadata_check_permissions(Fam_DataItem_Metadata *dataitem,
                                    metadata_region_item_op_t op, uint64_t uid,
                                    uint64_t gid);

    bool metadata_check_permissions(Fam_Region_Metadata *region,
                                    metadata_region_item_op_t op, uint64_t uid,
                                    uint64_t gid);

    int metadata_create_region_check(string regionname, size_t size,
                                     uint64_t *regionid,
                                     std::list<int> *memory_server_list,
                                     int user_policy);

    int metadata_destroy_region_check(const uint64_t regionId, uint64_t uid,
                                      uint64_t gid,
                                      std::list<int> *memory_server_list);

    int metadata_allocate_check(const std::string dataitemName,
                                const uint64_t regionId, uint32_t uid,
                                uint32_t gid);

    int metadata_deallocate_check(const uint64_t regionId,
                                  const uint64_t dataitemId, uint32_t uid,
                                  uint32_t gid);

    size_t metadata_maxkeylen();

    int metadata_get_RegionID(uint64_t *regionID);
    int metadata_reset_RegionID(uint64_t regionid);

    int metadata_update_memoryserver(Fam_memserver_info &memserver_info_list,
                                     int list_size);

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
    int memoryServerCount;

    bool use_meta_region;
    KvsMap *metadataKvsMap;
    pthread_mutex_t kvsMapLock;

    MemoryManager *memoryManager;

    GlobalPtr create_metadata_kvs_tree(size_t heap_size = METADATA_HEAP_SIZE,
                                       nvmm::PoolId heap_id = METADATA_HEAP_ID);

    KeyValueStore *open_metadata_kvs(GlobalPtr root,
                                     size_t heap_size = METADATA_HEAP_SIZE,
                                     uint64_t heap_id = METADATA_HEAP_ID);

    int get_dataitem_KVS(uint64_t regionId, KeyValueStore *&dataitemIdKVS,
                         KeyValueStore *&dataitemNameKVS);

    int insert_in_regionname_kvs(const std::string regionName,
                                 const std::string regionId);

    int insert_in_regionid_kvs(const std::string regionKey,
                               Fam_Region_Metadata *region, bool insert);

    int get_regionid_from_regionname_KVS(const std::string regionName,
                                         std::string &regionId);

    void init_poolId_bmap();

    std::list<int> get_memory_server(const std::string regionName, size_t size,
                                     int user_policy);

    std::list<int> get_memory_server_list(Fam_Region_Metadata region);
};

/*
 * Initialize the FAM metadata manager
 */
int FAM_Metadata_Manager::Impl_::Init(bool use_meta_reg) {

    memoryManager = MemoryManager::GetInstance();

    metadataKvsMap = new KvsMap();
    (void)pthread_mutex_init(&kvsMapLock, NULL);

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
    init_poolId_bmap();

    // TODO: Read memoryServerCount from KVS
    // As of now set memory server count as 1 in init.
    memoryServerCount = 1;

    return META_NO_ERROR;
}

int FAM_Metadata_Manager::Impl_::Final() {
    // Cleanup
    delete regionIdKVS;
    delete regionNameKVS;
    delete metadataKvsMap;
    pthread_mutex_destroy(&kvsMapLock);

    return META_NO_ERROR;
}

/*
 * Initalize the region Id bitmap address.
 * Size of bitmap is Max poolId's supported / 8 bytes.
 * Get the Bitmap address reserved from the root shelf.
 */
void FAM_Metadata_Manager::Impl_::init_poolId_bmap() {
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
FAM_Metadata_Manager::Impl_::create_metadata_kvs_tree(size_t heap_size,
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

KeyValueStore *
FAM_Metadata_Manager::Impl_::open_metadata_kvs(GlobalPtr root, size_t heap_size,
                                               uint64_t heap_id) {

    return KeyValueStore::MakeKVS(KVSTYPE, root, "", "", heap_size,
                                  (PoolId)heap_id);
}

int FAM_Metadata_Manager::Impl_::get_dataitem_KVS(
    uint64_t regionId, KeyValueStore *&dataitemIdKVS,
    KeyValueStore *&dataitemNameKVS) {

    int ret = META_NO_ERROR;
    pthread_mutex_lock(&kvsMapLock);
    auto kvsObj = metadataKvsMap->find(regionId);
    if (kvsObj == metadataKvsMap->end()) {
        pthread_mutex_unlock(&kvsMapLock);
        // If regionId is not present in KVS map, open the KVS using
        // root pointer and insert the KVS in the map
        Fam_Region_Metadata regNode;
        ret = metadata_find_region(regionId, regNode);
        if (ret != META_NO_ERROR) {
            return ret;
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
        kvs->diNameKVS = dataitemNameKVS;
        kvs->diIdKVS = dataitemIdKVS;
        pthread_mutex_lock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(regionId);
        if (kvsObj == metadataKvsMap->end()) {
            metadataKvsMap->insert({regionId, kvs});
        } else {
            pthread_mutex_unlock(&kvsMapLock);
            return META_NO_ERROR;
        }
        pthread_mutex_unlock(&kvsMapLock);
    } else {
        pthread_mutex_unlock(&kvsMapLock);
        dataitemIdKVS = (kvsObj->second)->diIdKVS;
        dataitemNameKVS = (kvsObj->second)->diNameKVS;
    }

    if ((dataitemIdKVS == nullptr) || (dataitemNameKVS == nullptr)) {
        DEBUG_STDERR(regionId, "KVS not found");
        return META_ERROR;
    }

    return ret;
}

/**
 * metadata_find_region - Lookup a region id in metadata region KVS
 * @param regionId - Region ID
 * @param region - return the Descriptor to the region if it exists
 * @return - META_NO_ERROR if key exists, META_KEY_DOES_NOT_EXIST if key not
 *	found
 *
 */
int FAM_Metadata_Manager::Impl_::metadata_find_region(
    const uint64_t regionId, Fam_Region_Metadata &region) {

    int ret;
    std::string regionKey = std::to_string(regionId);
    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    ret =
        regionIdKVS->Get(regionKey.c_str(), regionKey.size(), val_buf, val_len);
    if (ret == META_NO_ERROR) {

        memcpy((char *)&region, val_buf, sizeof(Fam_Region_Metadata));

        return META_NO_ERROR;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else {
        DEBUG_STDERR(regionId, ret);
        return META_ERROR;
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
int FAM_Metadata_Manager::Impl_::metadata_find_region(
    const std::string regionName, Fam_Region_Metadata &region) {

    int ret;

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    ret = regionNameKVS->Get(regionName.c_str(), regionName.size(), val_buf,
                             val_len);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else if (ret == META_NO_ERROR) {

        uint64_t regionID = strtoul(val_buf, NULL, 0);

        ret = metadata_find_region(regionID, region);

        if (ret == META_NO_ERROR) {
            return ret;
        } else if (ret == META_KEY_DOES_NOT_EXIST) {
            return ret;
        } else {
            DEBUG_STDERR(regionName, "Region id lookup failed");
            return META_ERROR;
        }
    } else {
        DEBUG_STDERR(regionName, "Get failed.");
        return META_ERROR;
    }
    return ret;
}

/**
 * insert_in_regionname_kvs - Helper function to create a region name to
 * 		region id KVS tree
 * @param regionName - Region Name
 * @param regionId - Region Id
 * @return - META_NO_ERROR if key added successfully, META_KEY_ALREADY_EXIST if
 * 	key already exists
 */
int FAM_Metadata_Manager::Impl_::insert_in_regionname_kvs(
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
int FAM_Metadata_Manager::Impl_::insert_in_regionid_kvs(
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
int FAM_Metadata_Manager::Impl_::get_regionid_from_regionname_KVS(
    const std::string regionName, std::string &regionId) {
    int ret;

    char val_buf[max_val_len];
    size_t val_len;

    ResetBuf(val_buf, val_len, max_val_len);

    ret = regionNameKVS->Get(regionName.c_str(), regionName.size(), val_buf,
                             val_len);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not forund");
        return ret;
    } else if (ret == META_NO_ERROR) {
        regionId.assign(val_buf, val_len);
        return ret;
    }
    return META_ERROR;
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
int FAM_Metadata_Manager::Impl_::metadata_insert_region(
    const uint64_t regionId, const std::string regionName,
    Fam_Region_Metadata *region) {

    int ret;

    std::string regionKey = std::to_string(regionId);

    // Insert region name -> region id mapping in regionDataKVS
    ret = insert_in_regionname_kvs(regionName, regionKey);
    if (ret == META_NO_ERROR) {
        // Region key does not exist, create an entry

        // Create a root entry for data item tree and insert the value
        // pointer
        GlobalPtr dataitemIdRoot, dataitemNameRoot;
        if (use_meta_region) {
            dataitemIdRoot = create_metadata_kvs_tree();
            dataitemNameRoot = create_metadata_kvs_tree();
        } else {
            dataitemIdRoot =
                create_metadata_kvs_tree(region->size, (PoolId)regionId);
            dataitemNameRoot =
                create_metadata_kvs_tree(region->size, (PoolId)regionId);
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
            return ret;
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        OPEN_METADATA_KVS(dataitemIdRoot, region->size, regionId,
                          dataitemIdKVS);
        if (dataitemIdKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            return META_ERROR;
        }

        OPEN_METADATA_KVS(dataitemNameRoot, region->size, regionId,
                          dataitemNameKVS);
        if (dataitemNameKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            return META_ERROR;
        }

        // Insert the KVS pointer into map
        diKVS *kvs = new diKVS();
        kvs->diNameKVS = dataitemNameKVS;
        kvs->diIdKVS = dataitemIdKVS;
        pthread_mutex_lock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(regionId);
        if (kvsObj == metadataKvsMap->end()) {
            metadataKvsMap->insert({regionId, kvs});
        } else {
            pthread_mutex_unlock(&kvsMapLock);
            return META_ERROR;
        }
        pthread_mutex_unlock(&kvsMapLock);

        return ret;
    } else if (ret == META_KEY_ALREADY_EXIST) {
        return ret;
    } else {
        DEBUG_STDERR(regionId, "Insert failed.");
        return META_ERROR;
    }

    return ret;
}

/**
 * metadata_modify_region - Update the Region descriptor in the
 * 	region ID metadata KVS.
 * @param regionId - Region Id
 * @param region -  Region descriptor to be added
 * @return - META_NO_ERROR if key added, META_KEY_DOES_NOT_EXIST if regionId
 * 	key not found
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_region(
    const uint64_t regionId, Fam_Region_Metadata *region) {

    int ret;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionId, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {
        // Region key does not exist
        DEBUG_STDOUT(regionId, "Region not found.");
        return ret;

    } else if (ret == META_NO_ERROR) {
        // KVS found... update the value of dataitem root from the region node
        std::string regionKey = std::to_string(regionId);

        // Insert the Region metadata descriptor in the region ID KVS
        ret = insert_in_regionid_kvs(regionKey, region, 0);

        return ret;

    } else {
        DEBUG_STDERR(regionId, "Region id lookup failed");
        return META_ERROR;
    }
    return ret;
}

/**
 * metadata_modify_region - Update the Region descriptor in the
 * 	region ID metadata KVS.
 * @param regionName - Region Id
 * @param region -  Region descriptor to be added
 * @return - META_NO_ERROR if key added, META_KEY_DOES_NOT_EXIST if regionId
 * 	key not found
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_region(
    const std::string regionName, Fam_Region_Metadata *region) {

    int ret;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionName, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {
        // Region key does not exist
        DEBUG_STDOUT(regionName, "Region not found.");
        return ret;

    } else if (ret == META_NO_ERROR) {
        // KVS found... update the value of dataitem root from the region node

        std::string regionKey = std::to_string(regNode.regionId);

        // Insert the Region metadata descriptor in the region ID KVS
        ret = insert_in_regionid_kvs(regionKey, region, 0);
        return ret;

    } else {
        DEBUG_STDERR(regionName, "Region name lookup failed");
        return META_ERROR;
    }
    return ret;
}

/**
 * metadata_delete_region - delete the Region key in the region metadata KVS
 * @param regionName - Name of the Region to be deleted
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if key not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_region(
    const std::string regionName) {

    int ret;

    std::string regionId;

    // Get the regionID from the region Name KVS
    ret = get_regionid_from_regionname_KVS(regionName, regionId);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not found.");
        return ret;
    } else if (ret == META_NO_ERROR) {
        pthread_mutex_lock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(stoull(regionId));
        if (kvsObj != metadataKvsMap->end()) {
            delete (kvsObj->second)->diIdKVS;
            delete (kvsObj->second)->diNameKVS;
            delete kvsObj->second;
            metadataKvsMap->erase(kvsObj);
        }
        pthread_mutex_unlock(&kvsMapLock);

        // Delete the entry from region ID KVS
        ret = regionIdKVS->Del(regionId.c_str(), regionId.size());

        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionName, "Region id not found.");
            return ret;
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionName, "Region id lookup failed.");
            return META_ERROR;
        }

        // delete the region Name -> region Id mapping from regin name KVS
        ret = regionNameKVS->Del(regionName.c_str(), regionName.size());

        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionName, "Region not found.");
            return ret;
        } else if (ret == META_NO_ERROR) {
            return ret;
        } else {
            DEBUG_STDERR(regionName, "Del failed.");
            return META_ERROR;
        }
        return ret;
    }

    return ret;
}

/**
 * metadata_delete_region - delete the Region key in the region metadata KVS
 * @param regionId - Region ID key to be deleted
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if key not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_region(
    const uint64_t regionId) {

    int ret;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionId, regNode);

    if (ret == META_NO_ERROR) {

        pthread_mutex_lock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(regionId);
        if (kvsObj != metadataKvsMap->end()) {
            delete (kvsObj->second)->diIdKVS;
            delete (kvsObj->second)->diNameKVS;
            delete kvsObj->second;
            metadataKvsMap->erase(kvsObj);
        }
        pthread_mutex_unlock(&kvsMapLock);

        // Get the region name for region metadata descriptor and
        // remove the name key from region Name KVS
        std::string regionName = regNode.name;

        ret = regionNameKVS->Del(regionName.c_str(), regionName.size());

        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionId, "Region not found");
            return ret;
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionId, "Del failed.");
            return META_ERROR;
        }

        // delete the region id for region ID KVS
        std::string regionKey = std::to_string(regionId);
        ret = regionIdKVS->Del(regionKey.c_str(), regionKey.size());

        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionId, "Region not found");
            return ret;
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionId, "Del failed.");
            return META_ERROR;
        }
        return ret;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionId, "Region lookup failed.");
        return ret;
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_insert_dataitem(
    const uint64_t dataitemId, std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    int ret;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionName.c_str(), regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {

        DEBUG_STDOUT(regionName, "region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemId, "FindOrCreate failed");
                return ret;
            }
        }

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->FindOrCreate(
            dataitemKey.c_str(), dataitemKey.size(), val_node,
            sizeof(Fam_DataItem_Metadata), val_buf, val_len);

        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(dataitemId, "FindOrCreate failed");
            // if entry was added in dataitem name KVS, then delete it
            if (!dataitemName.empty()) {
                int ret1 = dataitemNameKVS->Del(dataitemName.c_str(),
                                                dataitemName.size());
                if (ret1 != META_NO_ERROR) {
                    return ret1;
                }
            }
            return ret;
        }

        return ret;

    } else {

        DEBUG_STDERR(dataitemId, "Region lookup failed");
        return META_ERROR;
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    int ret;

    Fam_Region_Metadata regNode;
    ret = metadata_find_region(regionId, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {

        DEBUG_STDOUT(regionId, "region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemKey.c_str(), "FindOrCreate failed");
                DEBUG_STDERR(ret, "FindOrCreate failed");
                return META_ERROR;
            }
        }
        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->FindOrCreate(
            dataitemKey.c_str(), dataitemKey.size(), val_node,
            sizeof(Fam_DataItem_Metadata), val_buf, val_len);

        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(dataitemKey, "FindOrCreate failed.");
            // if entry was added in dataitem name KVS, then delete it
            if (!dataitemName.empty()) {
                int ret1 = dataitemNameKVS->Del(dataitemName.c_str(),
                                                dataitemName.size());
                if (ret1 != META_NO_ERROR) {
                    return ret1;
                }
            }
            return ret;
        }
        return ret;

    } else {

        DEBUG_STDERR(dataitemId, "Region lookup failed");
        return META_ERROR;
    }
    return ret;
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemId- dataitem Id of the dataitem descp to be modified
 * @param regionId - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *  	region not found or dataitem Id entry not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {

    int ret;

    // Check if dataitem exists
    // if no_error, update new entry
    // if not found, return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemId, regionId, dataitemNode);
    if (ret == META_NO_ERROR) {

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemId, "Put failed");
            return META_ERROR;
        }

        // Check if name was not assigned earlier and now name is being
        // assigned
        if ((strlen(dataitemNode.name) == 0) && (strlen(dataitem->name) > 0)) {

            std::string dataitemName = dataitem->name;
            ret =
                dataitemNameKVS->Put(dataitemName.c_str(), dataitemName.size(),
                                     dataitemKey.c_str(), dataitemKey.size());

            if (ret == META_ERROR) {
                DEBUG_STDERR(dataitemId, "Put failed");
                return META_ERROR;
            }
        }
        return ret;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    }

    return ret;
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemId- dataitem Id of the dataitem descp to be modified
 * @param regionName - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *	region not found or dataitem Id entry not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {

    int ret;

    // Check if dataitem exists
    // if no_error, update the entry

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemId, regionName, dataitemNode);
    if (ret == META_NO_ERROR) {

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            return ret;
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemId, "Put failed");
            return META_ERROR;
        }

        // Check if name was not assigned earlier and now name is being
        // assigned
        if ((strlen(dataitemNode.name) == 0) && (strlen(dataitem->name) > 0)) {

            if (dataitemNameKVS == nullptr) {
                DEBUG_STDERR(dataitemId, "KVS creation failed");
                return META_ERROR;
            }

            std::string dataitemName = dataitem->name;

            ret =
                dataitemNameKVS->Put(dataitemName.c_str(), dataitemName.size(),
                                     dataitemKey.c_str(), dataitemKey.size());
            if (ret == META_ERROR) {
                DEBUG_STDERR(dataitemId, "Put failed");
                return META_ERROR;
            }
        }
        return ret;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    }

    return ret;
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemName- dataitem name of the dataitem descp to be modified
 * @param regionID - Region  ID to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 * 	region not found or dataitem Id entry not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {

    int ret;

    // Check if dataitem exists
    // if no_error, update new entry
    // if not found, return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemName, regionId, dataitemNode);
    if (ret == META_NO_ERROR) {

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemName, "Put failed");
                return META_ERROR;
            }
        }

        return ret;
    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    }

    return ret;
}

/**
 * metadata_modify_dataitem - Update the dataitem descriptor in the dataitem KVS
 * @param dataitemName- dataitem name of the dataitem descp to be modified
 * @param regionName - Region Name to which dataitem belongs
 * @param dataitem -  dataitem metadata descriptor to be modified
 * @return - META_NO_ERROR if descriptor updated , META_KEY_DOES_NOT_EXIST if
 *           region not found or dataitem Id entry not found.
 */
int FAM_Metadata_Manager::Impl_::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {

    int ret;

    // Check if dataitem exists
    // if no_error, create new entry
    // return META_KEY_DOES_NOT_EXIST

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemName, regionName, dataitemNode);
    if (ret == META_NO_ERROR) {

        char val_node[sizeof(Fam_DataItem_Metadata) + 1];
        memcpy((char *)val_node, (char const *)dataitem,
               sizeof(Fam_DataItem_Metadata));

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            return ret;
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemName, "Put failed.");
                return META_ERROR;
            }
        }

        return ret;

    } else if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    }

    return ret;
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemId - dataitem Id in the region to be deleted
 * @param regionName - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {

    int ret;

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemId, regionName, dataitemNode);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else if (ret == META_NO_ERROR) {

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            return ret;
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemId, "Del failed.");
            return META_ERROR;
        }

        std::string dataitemName = dataitemNode.name;
        if (!dataitemName.empty()) {
            ret =
                dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
            if (ret == META_ERROR) {
                DEBUG_STDERR(dataitemId, "Del failed.");
                return META_ERROR;
            }
        }

        return ret;
    }
    return ret;
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemId - dataitem Id in the region to be deleted
 * @param regionId - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_dataitem(
    const uint64_t dataitemId, const uint64_t regionId) {

    int ret;

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemId, regionId, dataitemNode);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemId, "Del failed.");
            return META_ERROR;
        }

        std::string dataitemName = dataitemNode.name;
        if (!dataitemName.empty()) {
            ret =
                dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
            if (ret == META_ERROR) {
                DEBUG_STDERR(dataitemId, "Del failed.");
                return META_ERROR;
            }
        }
    }
    return ret;
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemName - dataitem Name in the region to be deleted
 * @param regionId - Id of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST if region or daatitem key does not exists
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {

    int ret;

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemName, regionId, dataitemNode);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemName, "Del failed.");
                return META_ERROR;
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemName, "Del failed.");
            return META_ERROR;
        }

        return ret;
    }
    return ret;
}

/**
 * metadata_delete_dataitem - delete the dataitem key from the dataitem KVS
 * @param dataitemName - dataitem Name in the region to be deleted
 * @param regionName - Name of the Region
 * @return - META_NO_ERROR if the key is deleted successfully,
 * META_KEY_DOES_NOT_EXIST
 * 	if region or daatitem key does not exists
 */
int FAM_Metadata_Manager::Impl_::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {

    int ret;

    Fam_DataItem_Metadata dataitemNode;
    ret = metadata_find_dataitem(dataitemName, regionName, dataitemNode);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        return ret;
    } else if (ret == META_NO_ERROR) {

        // Get the regionID from the region Name KVS
        std::string regionId;
        ret = get_regionid_from_regionname_KVS(regionName, regionId);
        if (ret != META_NO_ERROR) {
            DEBUG_STDERR(regionName, "Region not found");
            return ret;
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
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
                DEBUG_STDERR(dataitemName, "Del failed.");
                return META_ERROR;
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemName, "Del failed.");
            return META_ERROR;
        }
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionId, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionId, "Region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));
        } else {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return ret;
        }
        return ret;
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionName, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));

        } else {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return ret;
        }
        return ret;
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionId, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionId, "Region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey;
        char val_buf[max_val_len];
        size_t val_len;
        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_KEY_DOES_NOT_EXIST) {

            return ret;
        } else if (ret == META_NO_ERROR) {

            dataitemKey.assign(val_buf, val_len);
            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                     val_buf, val_len);
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));

            } else {
                DEBUG_STDERR(dataitemName, "Get failed.");
                return ret;
            }
        }
        return ret;
    }
    return ret;
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
int FAM_Metadata_Manager::Impl_::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    char val_buf[max_val_len];
    size_t val_len;

    Fam_Region_Metadata regNode;

    ret = metadata_find_region(regionName, regNode);
    if (ret == META_KEY_DOES_NOT_EXIST) {

        DEBUG_STDOUT(regionName, "Region not found");
        return ret;

    } else if (ret == META_NO_ERROR) {

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            return ret;
        }

        std::string dataitemKey;

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemNameKVS->Get(dataitemName.c_str(), dataitemName.size(),
                                   val_buf, val_len);

        if (ret == META_KEY_DOES_NOT_EXIST) {
            return ret;
        } else if (ret == META_NO_ERROR) {

            dataitemKey.assign(val_buf, val_len);

            ResetBuf(val_buf, val_len, max_val_len);

            ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                     val_buf, val_len);
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));

            } else {
                DEBUG_STDERR(dataitemName, "Get failed.");
                return ret;
            }
        }
        return ret;
    }
    return ret;
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
bool FAM_Metadata_Manager::Impl_::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint64_t uid,
    uint64_t gid) {

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
bool FAM_Metadata_Manager::Impl_::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint64_t uid,
    uint64_t gid) {

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

size_t FAM_Metadata_Manager::Impl_::metadata_maxkeylen() {
    return regionNameKVS->MaxKeyLen();
}

int FAM_Metadata_Manager::Impl_::metadata_get_RegionID(uint64_t *regionID) {
    // Find the first free bit after 10th bit in poolId bitmap.
    // First 10 nvmm ID will be reserved.
    *regionID = bitmap_find_and_reserve(bmap, 0, MEMSERVER_REGIONID_START);
    if (*regionID == (uint64_t)BITMAP_NOTFOUND)
        return META_NO_FREE_REGIONID;

    return META_NO_ERROR;
}

int FAM_Metadata_Manager::Impl_::metadata_reset_RegionID(uint64_t regionID) {
    bitmap_reset(bmap, regionID);
    return META_NO_ERROR;
}

/**
 * metadata_create_region_check - Check if region name exists and find
 *     regionid and memoryservers.
 * @params regionname - Region name to create
 * @params size - Size of the region
 * @params regionid - Region id of the region
 * @params memory_server_list - List of memory server in which this region
 * should be created.
 * @params userpolicy - User policy recommendation
 *
 */

int FAM_Metadata_Manager::Impl_::metadata_create_region_check(
    const std::string regionname, size_t size, uint64_t *regionid,
    std::list<int> *memory_server_list, int user_policy = 0) {

    Fam_Region_Metadata region;
    //
    // Check if name length is within limit
    if (regionname.size() > metadata_maxkeylen())
        return META_LARGE_NAME;

    // Check if region with that name exist
    int ret = metadata_find_region(regionname, region);
    if (ret == META_NO_ERROR) {
        return META_KEY_ALREADY_EXIST;
    }

    // Call metadata_get_RegionID to get regionID
    ret = metadata_get_RegionID(regionid);
    if (ret != META_NO_ERROR) {
        return ret;
    }

    // Call find_memory_server for the size asked for and for user policy
    *memory_server_list = get_memory_server(regionname, size, user_policy);
    return META_NO_ERROR;
}

/**
 * metadata_destroy_region_check - Remove the region from metadata
 * @params regionname - Region id to destroy
 * @params uid - User id of the user
 * @params gid - gid of the user
 * @params memory_server_list - List of memory server from which the region
 * should be removed.
 *
 */
int FAM_Metadata_Manager::Impl_::metadata_destroy_region_check(
    const uint64_t regionId, uint64_t uid, uint64_t gid,
    std::list<int> *memory_server_list) {
    // Check if region exists
    // Check if the region exist, if not return error
    Fam_Region_Metadata region;
    int ret = metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        return ret;
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            return META_NO_PERMISSION;
        }
    }

    // remove region from metadata service.
    // metadata_delete_region() is called before DestroyHeap() as
    // cached KVS is freed in metadata_delete_region and calling
    // metadata_delete_region after DestroyHeap will result in SIGSEGV.

    ret = metadata_delete_region(regionId);
    if (ret != META_NO_ERROR) {
        return ret;
    }

    // Return memory server list to user
    *memory_server_list = get_memory_server_list(region);
    return META_NO_ERROR;
}

std::list<int>
FAM_Metadata_Manager::Impl_::get_memory_server(const std::string regionname,
                                               size_t size, int user_policy) {
    std::uint64_t hashVal = std::hash<std::string>{}(regionname);
    return {(int)hashVal % memoryServerCount};
}

std::list<int> FAM_Metadata_Manager::Impl_::get_memory_server_list(
    Fam_Region_Metadata region) {
    std::uint64_t hashVal = std::hash<std::string>{}(region.name);
    return {(int)hashVal % memoryServerCount};
}

int FAM_Metadata_Manager::Impl_::metadata_allocate_check(
    const std::string dataitemName, const uint64_t regionId, uint32_t uid,
    uint32_t gid) {

    int ret;
    // Check if the name size is bigger than MAX_KEY_LEN supported
    if (dataitemName.size() > metadata_maxkeylen()) {
        return META_LARGE_NAME;
    }

    // Check with metadata service if the region exist, if not return error
    Fam_Region_Metadata region;
    ret = metadata_find_region(regionId, region);
    if (ret != META_NO_ERROR) {
        return ret;
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to create dataitem in that region, if not return error
    if (uid != region.uid) {
        bool isPermitted = metadata_check_permissions(
            &region, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            return META_NO_PERMISSION;
        }
    }

    // Check with metadata service if data item with the requested name
    // is already exist, if exists return error
    Fam_DataItem_Metadata dataitem;
    if (dataitemName != "") {
        ret = metadata_find_dataitem(dataitemName, regionId, dataitem);
        if (ret == META_NO_ERROR) {
            return META_KEY_ALREADY_EXIST;
        }
    }
    return META_NO_ERROR;
}

int FAM_Metadata_Manager::Impl_::metadata_deallocate_check(
    const uint64_t regionId, const uint64_t dataitemId, uint32_t uid,
    uint32_t gid) {
    // Check with metadata service if data item with the requested name
    // is already exist, if not return error
    Fam_DataItem_Metadata dataitem;
    int ret = metadata_find_dataitem(dataitemId, regionId, dataitem);
    if (ret != META_NO_ERROR) {
        return ret;
    }

    // Check if calling PE user is owner. If not, check with
    // metadata service if the calling PE has the write
    // permission to destroy region, if not return error
    if (uid != dataitem.uid) {
        bool isPermitted = metadata_check_permissions(
            &dataitem, META_REGION_ITEM_WRITE, uid, gid);
        if (!isPermitted) {
            return META_NO_PERMISSION;
        }
    }

    // Remove data item from metadata service
    ret = metadata_delete_dataitem(dataitemId, regionId);
    if (ret != META_NO_ERROR) {
        return ret;
    }
    return META_NO_ERROR;
}

int FAM_Metadata_Manager::Impl_::metadata_update_memoryserver(
    Fam_memserver_info &memserver_info_list, int list_size) {
    memoryServerCount = list_size;
    // TODO: Update KVS with info.
    return NO_ERROR;
}

/*
 * Public APIs of FAM_Metadata_Manager
 */

FAM_Metadata_Manager *FAM_Metadata_Manager::GetInstance(bool use_meta_reg) {
    static FAM_Metadata_Manager instance(use_meta_reg);
    return &instance;
}

FAM_Metadata_Manager::FAM_Metadata_Manager(bool use_meta_reg) {
    Start(use_meta_reg);
}

FAM_Metadata_Manager::~FAM_Metadata_Manager() { Stop(); }

void FAM_Metadata_Manager::Stop() {
    int ret = pimpl_->Final();
    assert(ret == META_NO_ERROR);
    if (pimpl_)
        delete pimpl_;
}

void FAM_Metadata_Manager::Start(bool use_meta_reg) {

    MEMSERVER_PROFILE_INIT(METADATA)
    MEMSERVER_PROFILE_START_TIME(METADATA)
    pimpl_ = new Impl_;
    assert(pimpl_);
    int ret = pimpl_->Init(use_meta_reg);
    assert(ret == META_NO_ERROR);
}

void FAM_Metadata_Manager::reset_profile() {
    MEMSERVER_PROFILE_INIT(METADATA)
    MEMSERVER_PROFILE_START_TIME(METADATA)
}

void FAM_Metadata_Manager::dump_profile() { METADATA_PROFILE_DUMP(); }

int FAM_Metadata_Manager::metadata_insert_region(const uint64_t regionID,
                                                 const std::string regionName,
                                                 Fam_Region_Metadata *region) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_insert_region(regionID, regionName, region);
    METADATA_PROFILE_END_OPS(metadata_insert_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_region(const uint64_t regionID) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_region(regionID);
    METADATA_PROFILE_END_OPS(metadata_delete_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_region(const std::string regionName) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_region(regionName);
    METADATA_PROFILE_END_OPS(metadata_delete_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_region(const std::string regionName,
                                               Fam_Region_Metadata &region) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_region(regionName, region);
    METADATA_PROFILE_END_OPS(metadata_find_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_region(const uint64_t regionId,
                                               Fam_Region_Metadata &region) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_region(regionId, region);
    METADATA_PROFILE_END_OPS(metadata_find_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_modify_region(const uint64_t regionID,
                                                 Fam_Region_Metadata *region) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_region(regionID, region);
    METADATA_PROFILE_END_OPS(metadata_modify_region);
    return ret;
}
int FAM_Metadata_Manager::metadata_modify_region(const std::string regionName,
                                                 Fam_Region_Metadata *region) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_region(regionName, region);
    METADATA_PROFILE_END_OPS(metadata_modify_region);
    return ret;
}

int FAM_Metadata_Manager::metadata_insert_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_insert_dataitem(dataitemId, regionName, dataitem,
                                           dataitemName);
    METADATA_PROFILE_END_OPS(metadata_insert_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_insert_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem, std::string dataitemName) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_insert_dataitem(dataitemId, regionId, dataitem,
                                           dataitemName);
    METADATA_PROFILE_END_OPS(metadata_insert_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_dataitem(
    const uint64_t dataitemId, const std::string regionName) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_dataitem(dataitemId, regionName);
    METADATA_PROFILE_END_OPS(metadata_delete_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_dataitem(const uint64_t dataitemId,
                                                   const uint64_t regionId) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_dataitem(dataitemId, regionId);
    METADATA_PROFILE_END_OPS(metadata_delete_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_dataitem(
    const std::string dataitemName, const std::string regionName) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_dataitem(dataitemName, regionName);
    METADATA_PROFILE_END_OPS(metadata_delete_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_delete_dataitem(
    const std::string dataitemName, const uint64_t regionId) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_delete_dataitem(dataitemName, regionId);
    METADATA_PROFILE_END_OPS(metadata_delete_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemId, regionId, dataitem);
    METADATA_PROFILE_END_OPS(metadata_find_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemId, regionName, dataitem);
    METADATA_PROFILE_END_OPS(metadata_find_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemName, regionId, dataitem);
    METADATA_PROFILE_END_OPS(metadata_find_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_find_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata &dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_dataitem(dataitemName, regionName, dataitem);
    METADATA_PROFILE_END_OPS(metadata_find_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_modify_dataitem(
    const uint64_t dataitemId, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_dataitem(dataitemId, regionId, dataitem);
    METADATA_PROFILE_END_OPS(metadata_modify_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_modify_dataitem(
    const uint64_t dataitemId, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_dataitem(dataitemId, regionName, dataitem);
    METADATA_PROFILE_END_OPS(metadata_modify_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_modify_dataitem(
    const std::string dataitemName, const uint64_t regionId,
    Fam_DataItem_Metadata *dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_dataitem(dataitemName, regionId, dataitem);
    METADATA_PROFILE_END_OPS(metadata_modify_dataitem);
    return ret;
}

int FAM_Metadata_Manager::metadata_modify_dataitem(
    const std::string dataitemName, const std::string regionName,
    Fam_DataItem_Metadata *dataitem) {

    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_modify_dataitem(dataitemName, regionName, dataitem);
    METADATA_PROFILE_END_OPS(metadata_modify_dataitem);
    return ret;
}

bool FAM_Metadata_Manager::metadata_check_permissions(
    Fam_DataItem_Metadata *dataitem, metadata_region_item_op_t op, uint64_t uid,
    uint64_t gid) {

    bool ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_check_permissions(dataitem, op, uid, gid);
    METADATA_PROFILE_END_OPS(metadata_check_permissions);
    return ret;
}

bool FAM_Metadata_Manager::metadata_check_permissions(
    Fam_Region_Metadata *region, metadata_region_item_op_t op, uint64_t uid,
    uint64_t gid) {

    bool ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_check_permissions(region, op, uid, gid);
    METADATA_PROFILE_END_OPS(metadata_check_permissions);
    return ret;
}

size_t FAM_Metadata_Manager::metadata_maxkeylen() {

    size_t ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_maxkeylen();
    METADATA_PROFILE_END_OPS(metadata_maxkeylen);
    return ret;
}

int FAM_Metadata_Manager::metadata_create_region_check(
    string regionname, size_t size, uint64_t *regionid,
    std::list<int> *memory_server_list, int user_policy) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_create_region_check(regionname, size, regionid,
                                               memory_server_list, user_policy);
    METADATA_PROFILE_END_OPS(metadata_create_region_check);
    return ret;
}

int FAM_Metadata_Manager::metadata_destroy_region_check(
    const uint64_t regionId, uint32_t uid, uint32_t gid,
    std::list<int> *memory_server_list) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_destroy_region_check(regionId, uid, gid,
                                                memory_server_list);
    METADATA_PROFILE_END_OPS(metadata_destroy_region_check);
    return ret;
}

int FAM_Metadata_Manager::metadata_allocate_check(
    const std::string dataitemName, const uint64_t regionId, uint32_t uid,
    uint32_t gid) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_allocate_check(dataitemName, regionId, uid, gid);
    METADATA_PROFILE_END_OPS(metadata_allocate_check);
    return ret;
}

int FAM_Metadata_Manager::metadata_deallocate_check(const uint64_t regionId,
                                                    const uint64_t dataitemId,
                                                    uint32_t uid,
                                                    uint32_t gid) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_deallocate_check(regionId, dataitemId, uid, gid);
    METADATA_PROFILE_END_OPS(metadata_deallocate_check);
    return ret;
}

int FAM_Metadata_Manager::metadata_reset_RegionID(uint64_t regionid) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_reset_RegionID(regionid);
    METADATA_PROFILE_END_OPS(metadata_reset_RegionID);
    return ret;
}

int FAM_Metadata_Manager::metadata_get_RegionID(uint64_t *regionid) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_get_RegionID(regionid);
    METADATA_PROFILE_END_OPS(metadata_get_RegionID);
    return ret;
}

int FAM_Metadata_Manager::metadata_update_memoryserver(
    Fam_memserver_info &memserver_info_list, int list_size) {
    int ret;
    METADATA_PROFILE_START_OPS()
    ret = pimpl_->metadata_update_memoryserver(memserver_info_list, list_size);
    METADATA_PROFILE_END_OPS(metadata_update_memoryserver);
    return ret;
}
} // end namespace metadata

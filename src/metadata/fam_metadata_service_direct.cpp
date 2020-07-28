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
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string.h>
#include <unistd.h>

#include "common/fam_memserver_profile.h"

#define OPEN_METADATA_KVS(root, heap_size, heap_id, kvs)                       \
    if (use_meta_region) {                                                     \
        kvs = open_metadata_kvs(root);                                         \
    } else {                                                                   \
        kvs = open_metadata_kvs(root, heap_size, heap_id);                     \
    }

using namespace famradixtree;
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

void metadata_direct_profile_dump(){
    MEMSERVER_PROFILE_END(METADATA_DIRECT)
        MEMSERVER_DUMP_PROFILE_BANNER(METADATA_DIRECT)
#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_DUMP_PROFILE_DATA(METADATA_DIRECT, name, prof_##name)
#include "metadata/metadata_direct_counters.tbl"

#undef MEMSERVER_COUNTER
#define MEMSERVER_COUNTER(name)                                                \
    MEMSERVER_PROFILE_TOTAL(METADATA_DIRECT, prof_##name)
#include "metadata/metadata_direct_counters.tbl"
            MEMSERVER_DUMP_PROFILE_SUMMARY(METADATA_DIRECT)}

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

    int Init(bool use_meta_reg);

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

    size_t metadata_maxkeylen();

  private:
    // KVS for region Id tree
    KeyValueStore *regionIdKVS;
    // KVS for region Name tree
    KeyValueStore *regionNameKVS;
    // KVS root pointer for region Id tree
    GlobalPtr regionIdRoot;
    // KVS root pointer for region Name tree
    GlobalPtr regionNameRoot;

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
};

/*
 * Initialize the FAM metadata manager
 */
int Fam_Metadata_Service_Direct::Impl_::Init(bool use_meta_reg) {

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

    return META_NO_ERROR;
}

int Fam_Metadata_Service_Direct::Impl_::Final() {
    // Cleanup
    delete regionIdKVS;
    delete regionNameKVS;
    delete metadataKvsMap;
    pthread_mutex_destroy(&kvsMapLock);

    return META_NO_ERROR;
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
    KeyValueStore *&dataitemNameKVS) {

    int ret = META_NO_ERROR;
    pthread_mutex_lock(&kvsMapLock);
    auto kvsObj = metadataKvsMap->find(regionId);
    if (kvsObj == metadataKvsMap->end()) {
        pthread_mutex_unlock(&kvsMapLock);
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
void Fam_Metadata_Service_Direct::Impl_::metadata_insert_region(
    const uint64_t regionId, const std::string regionName,
    Fam_Region_Metadata *region) {

    ostringstream message;
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
            message << "Region Id insertion failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        KeyValueStore *dataitemIdKVS, *dataitemNameKVS;
        OPEN_METADATA_KVS(dataitemIdRoot, region->size, regionId,
                          dataitemIdKVS);
        if (dataitemIdKVS == nullptr) {
            DEBUG_STDERR(regionId, "KVS creation failed");
            message << "Dataitem Id KVS creation failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        OPEN_METADATA_KVS(dataitemNameRoot, region->size, regionId,
                          dataitemNameKVS);
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
        pthread_mutex_lock(&kvsMapLock);
        auto kvsObj = metadataKvsMap->find(regionId);
        if (kvsObj == metadataKvsMap->end()) {
            metadataKvsMap->insert({regionId, kvs});
        } else {
            pthread_mutex_unlock(&kvsMapLock);
            delete kvs;
            message << "KVS pointers already exist in map";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
        pthread_mutex_unlock(&kvsMapLock);
    } else if (ret == META_KEY_ALREADY_EXIST) {
        message << "Region already exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
    } else {
        // KVS found... update the value of dataitem root from the region node
        std::string regionKey = std::to_string(regionId);

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
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
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

    int ret;

    std::string regionId;

    // Get the regionID from the region Name KVS
    ret = get_regionid_from_regionname_KVS(regionName, regionId);

    if (ret == META_KEY_DOES_NOT_EXIST) {
        DEBUG_STDOUT(regionName, "Region not found.");
        message << "Regioin does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                        message.str().c_str());
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
            message << "Regioin does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionName, "Region id lookup failed.");
            message << "Region Id entry removal failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // delete the region Name -> region Id mapping from regin name KVS
        ret = regionNameKVS->Del(regionName.c_str(), regionName.size());

        if (ret == META_KEY_DOES_NOT_EXIST) {
            DEBUG_STDOUT(regionName, "Region not found.");
            message << "Regioin does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        } else if (ret == META_NO_ERROR) {
            return;
        } else {
            DEBUG_STDERR(regionName, "Del failed.");
            message << "Region name entry removal failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
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
            message << "Regioin does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
            message << "Regioin does not exist";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        } else if (ret == META_ERROR) {
            DEBUG_STDERR(regionId, "Del failed.");
            message << "Region Id entry removal failed";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionId, "Region lookup failed.");
        message << "Regioin does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemId, "FindOrCreate failed");
                message << "Failed to insert metadata into dataitem name KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
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
                    message << "Failed to remove dataitem name entry from KVS, "
                               "while cleanup";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                    message.str().c_str());
                }
            }
            message << "Failed to insert metadata into dataitem Id KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionName, "region not found");
        message << "Regioin does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemName, "FindOrCreate failed");
                message << "Failed to insert metadata into dataitem name KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
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
                    message << "Failed to remove dataitem name entry from KVS, "
                               "while cleanup";
                    THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                    message.str().c_str());
                }
            }
            message << "Failed to insert metadata into dataitem Id KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        DEBUG_STDOUT(regionId, "region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
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
                DEBUG_STDERR(dataitemId, "Put failed");
                message << "Failed to insert dataitem name entry";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ret = dataitemIdKVS->Put(dataitemKey.c_str(), dataitemKey.size(),
                                 val_node, sizeof(Fam_DataItem_Metadata));

        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemId, "Put failed");
            message << "Failed to update dataitem metadata";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        // Check if name was not assigned earlier and now name is being
        // assigned
        if ((strlen(dataitemNode.name) == 0) && (strlen(dataitem->name) > 0)) {

            if (dataitemNameKVS == nullptr) {
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
                DEBUG_STDERR(dataitemId, "Put failed");
                message << "Failed to insert dataitem name entry";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemName, "Put failed");
                message << "Failed to update dtaitem metadata";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemName, "Put failed.");
                message << "Failed to update dtaitem metadata";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
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
                DEBUG_STDERR(dataitemId, "Del failed.");
                message << "Failed to remove dataitem name entry from KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);
        ret = dataitemIdKVS->Del(dataitemKey.c_str(), dataitemKey.size());
        if (ret == META_ERROR) {
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
                DEBUG_STDERR(dataitemId, "Del failed.");
                message << "Failed to remove dataitem name entry from KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemName, "Del failed.");
                message << "Failed to remove metadata from dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemName, "Del failed.");
            message << "Failed to remove dataitem name entry from KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(stoull(regionId), dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
                DEBUG_STDERR(dataitemName, "Del failed.");
                message << "Failed to remove metadata from dataitem Id KVS";
                THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                                message.str().c_str());
            }
        }

        ret = dataitemNameKVS->Del(dataitemName.c_str(), dataitemName.size());
        if (ret == META_ERROR) {
            DEBUG_STDERR(dataitemName, "Del failed.");
            message << "Failed to remove dataitem name entry from KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }
    } else {
        message << "Dataitem does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));
            return true;
        } else {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        }
    } else {
        DEBUG_STDOUT(regionId, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
            message << "Failed to get dataitem KVS";
            THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
                            message.str().c_str());
        }

        std::string dataitemKey = std::to_string(dataitemId);

        ResetBuf(val_buf, val_len, max_val_len);

        ret = dataitemIdKVS->Get(dataitemKey.c_str(), dataitemKey.size(),
                                 val_buf, val_len);
        if (ret == META_NO_ERROR) {
            memcpy((char *)&dataitem, (char const *)val_buf,
                   sizeof(Fam_DataItem_Metadata));
            return true;
        } else {
            DEBUG_STDERR(dataitemId, "Get failed.");
            return false;
        }
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret = get_dataitem_KVS(regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));
                return true;
            } else {
                DEBUG_STDERR(dataitemName, "Get failed.");
                return false;
            }
        }
        return false;
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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
        ret =
            get_dataitem_KVS(regNode.regionId, dataitemIdKVS, dataitemNameKVS);
        if (ret != META_NO_ERROR) {
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
            if (ret == META_NO_ERROR) {
                memcpy((char *)&dataitem, (char const *)val_buf,
                       sizeof(Fam_DataItem_Metadata));
                return true;
            } else {
                DEBUG_STDERR(dataitemName, "Get failed.");
                return false;
            }
        }
        return false;
    } else {
        DEBUG_STDOUT(regionName, "Region not found");
        message << "Region does not exist";
        THROW_ERRNO_MSG(Metadata_Service_Exception, METADATA_ERROR,
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

Fam_Metadata_Service_Direct::Fam_Metadata_Service_Direct(bool use_meta_reg) {
    Start(use_meta_reg);
}

Fam_Metadata_Service_Direct::~Fam_Metadata_Service_Direct() { Stop(); }

void Fam_Metadata_Service_Direct::Stop() {
    int ret = pimpl_->Final();
    assert(ret == META_NO_ERROR);
    if (pimpl_)
        delete pimpl_;
}

void Fam_Metadata_Service_Direct::Start(bool use_meta_reg) {

    MEMSERVER_PROFILE_INIT(METADATA_DIRECT)
    MEMSERVER_PROFILE_START_TIME(METADATA_DIRECT)
    StartNVMM();
    pimpl_ = new Impl_;
    assert(pimpl_);
    int ret = pimpl_->Init(use_meta_reg);
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

void Fam_Metadata_Service_Direct::metadata_delete_region(
    const uint64_t regionID) {
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

bool Fam_Metadata_Service_Direct::metadata_find_region(
    const std::string regionName, Fam_Region_Metadata &region) {
    bool ret;
    METADATA_DIRECT_PROFILE_START_OPS()
    ret = pimpl_->metadata_find_region(regionName, region);
    METADATA_DIRECT_PROFILE_END_OPS(direct_metadata_find_region);
    return ret;
}

bool Fam_Metadata_Service_Direct::metadata_find_region(
    const uint64_t regionId, Fam_Region_Metadata &region) {
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

void Fam_Metadata_Service_Direct::metadata_delete_dataitem(
    const uint64_t dataitemId, const uint64_t regionId) {

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

} // end namespace metadata

/*
 * Copyright (c) 2022 Hewlett Packard Enterprise Development, LP. All
 * rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or
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
#ifndef FAM_C_H_
#define FAM_C_H_

#include <stdint.h>
#include <sys/stat.h>

#ifndef int128_t
#ifdef __int128
typedef __int128 int128_t;
#else
#ifdef __SIZEOF_INT128__
typedef __int128 int128_t;
#else
typedef long long int128_t;
#endif // __SIZEOF_INT128__
#endif // __int128
#endif // int128_t

// This header is being used in both C & C++. Hence, the extern "C"
// is kept under __cplusplus macro.
#ifdef __cplusplus
extern "C" {
#endif

typedef void c_fam;
typedef void c_fam_region_desc;
typedef void c_fam_desc;
typedef void c_fam_context;

typedef struct {
    char *defaultRegionName;
    char *cisServer;
    char *grpcPort;
    char *libfabricProvider;
    char *famThreadModel;
    char *cisInterfaceType;
    char *openFamModel;
    char *famContextModel;
    char *numConsumer;
    char *runtime;
    char *fam_default_memory_type;
    char *if_device;
} c_fam_options;

typedef enum { 
    INTERLEAVE_DEFAULT = 0, 
    ENABLE, 
    DISABLE 
} c_fam_Interleave_Enable;

typedef enum { 
    MEMORY_TYPE_DEFAULT = 0, 
    VOLATILE, 
    PERSISTENT 
} c_fam_Memory_Type;

typedef enum { 
    REDUNDANCY_LEVEL_DEFAULT = 0, 
    NONE, 
    RAID1, 
    RAID5 
} c_fam_Redundancy_Level;

typedef struct {
    c_fam_Redundancy_Level redundancyLevel;
    c_fam_Memory_Type memoryType;
    c_fam_Interleave_Enable interleaveEnable;
} c_fam_Region_Attributes;

typedef struct {
    uint64_t size;
    mode_t perm;
    char* name;
    uint32_t uid;
    uint32_t gid;
    c_fam_Region_Attributes region_attributes;
    uint64_t num_memservers;
    uint64_t interleaveSize;
    uint64_t memory_servers[256];
} c_fam_stat;

typedef struct {
    uint32_t backup_option_reserved;
} c_fam_backup_options;

/* This method is used to create a FAM instance 
 * and returns a pointer to the FAM instance.
 * @return - pointer to FAM instance
 */
c_fam* c_fam_create(void);

/**
 * Initialize the OpenFAM library. This method is required to be the first
 * method called when a process uses the OpenFAM library.
 * @param fam_obj - FAM instance
 * @param group_name - name of the group of cooperating PEs.
 * @param fam_opts - options structure containing initialization choices
 * @return - 0 on success and -1 on failure
 * @see #c_fam_finalize()
 * @see #c_Fam_Options
 */
int  c_fam_initialize(c_fam* fam_obj, const char* group_name, c_fam_options* fam_opts);

/**
 * Finalize the fam library. Once finalized, the process can continue work,
 * but it is disconnected from the OpenFAM library functions.
 * @param fam_obj - FAM instance
 * @param group_name - name of group of cooperating PEs for this job
 * @return - 0 on success and -1 on failure
 * @see #c_fam_initialize()
 */
int  c_fam_finalize(c_fam* fam_obj, const char* group_name);

/**
 * Allocate a large region of FAM. Regions are primarily used as large
 * containers within which additional memory may be allocated and managed by
 * the program. The API is extensible to support additional (implementation
 * dependent) options. Regions are long-lived and are automatically
 * registered with the name service.
 * @param fam_obj - FAM instance
 * @param region_name - name of the region
 * @param size - size (in bytes) requested for the region. Note that
 * implementations may round up the size to implementation-dependent sizes,
 * and may impose system-wide (or user-dependent) limits on individual and
 * total size allocated to a given user.
 * @param perm - access permissions to be used for the region
 * @param reg_att - details of fam region attributes
 * @return - Region_Descriptor for the created region
 * @see #c_fam_resize_region
 * @see #c_fam_destroy_region
 */
c_fam_region_desc* c_fam_create_region(c_fam* fam_obj, const char* region_name,
                                       size_t size, mode_t perm, c_fam_Region_Attributes* reg_att);

/**
 * Look up a region in FAM by name in the name service.
 * @param fam_obj - FAM instance
 * @param name - name of the region.
 * @return - The descriptor to the region. Null if no such region exists, or
 * if the caller does not have access.
 * @see #c_fam_lookup
 */
c_fam_region_desc* c_fam_lookup_region(c_fam* fam_obj, const char* name);

/**
 * Destroy a region, and all contents within the region. Note that this
 * method call will trigger a delayed free operation to permit other
 * instances currently using the region to finish.
 * @param descriptor - descriptor for the region
 * @return - 0 on success and -1 on failure
 * @see #fam_create_region
 * @see #fam_resize_region
 */
int c_fam_destroy_region(c_fam* fam_obj, c_fam_region_desc* region_desc);

/**
 * Allocate some unnamed space within a region. Allocates an area of FAM
 * within a region
 * @param fam_obj - FAM instance
 * @param size - size of the space to allocate in bytes.
 * @param perm - permissions associated with this space
 * @param region_desc - descriptor of the region within which the space is being
 * allocated. If not present or null, a default region is used.
 * @return - descriptor that can be used within the program to refer to this
 * space
 * @see #c_fam_deallocate()
 */
c_fam_desc* c_fam_allocate(c_fam* fam_obj, size_t size, mode_t perm,
                           c_fam_region_desc* region_desc);
/**
 * Allocate some unnamed space within a region. Allocates an area of FAM
 * within a region
 * @param fam_obj - FAM instance
 * @param name - name of the data item
 * @param size - size of the space to allocate in bytes.
 * @param perm - permissions associated with this space
 * @param region_desc - descriptor of the region within which the space is being
 * allocated. If not present or null, a default region is used.
 * @return - descriptor that can be used within the program to refer to this
 * space
 * @see #c_fam_deallocate()
 */
c_fam_desc* c_fam_allocate_named(c_fam* fam_obj, const char* name, size_t size,
                                 mode_t perm, c_fam_region_desc* region_desc);
/**
 * look up a data item in FAM by name in the name service.
 * @param fam_obj - FAM instance
 * @param name - name of the data item
 * @param region_name - name of the region containing the data item
 * @return descriptor to the data item if found. Null if no such data item
 * is registered, or if the caller does not have access.
 * @see #c_fam_lookup_region
 */
c_fam_desc* c_fam_lookup(c_fam* fam_obj, const char* name, const char* region_name);

/**
 * Copy data from one FAM-resident data item to another FAM-resident data
 * item (potentially within a different region).
 * @pararm fam_obj - FAM instance
 * @param src_desc - valid descriptor to source data item in FAM.
 * @param src_offset - byte offset within the space defined by the src
 * descriptor from which memory should be copied.
 * @param dest_desc - valid descriptor to destination data item in FAM.
 * @param dest_offset - byte offset within the space defined by the dest
 * descriptor to which memory should be copied.
 * @param size - number of bytes to be copied
 * @return - a pointer to the wait object for the copy operation
 * @see #c_fam_copy_wait
 */
void* c_fam_copy(c_fam* fam_obj, c_fam_desc* src_desc, uint64_t src_offset, c_fam_desc* dest_desc, uint64_t dest_offset, size_t size);

/**
 * Wait for copy operation correspond to the wait object passed to complete
 * @param fam_obj - FAM instance
 * @param wait_obj - unique tag to copy operation
 * @return - 0 on success and -1 on failure
 */
int  c_fam_copy_wait(c_fam* fam_obj, void* wait_obj);

/* Backup data item to archival storage.
 * @param fam_obj - FAM instance
 * @param src_desc - Data item to be backed up.
 * @param backup_name - Name of the backup
 * @param backup_options - backup related info.
 * @return - a pointer to the wait object for the backup operation.
 */
void* c_fam_backup(c_fam* fam_obj, c_fam_desc* src_desc, const char* backup_name, c_fam_backup_options* backup_options);

/**
 * Wait for backup operation corresponding to the wait object passed to
 * complete
 * @param fam_obj - FAM instance
 * @param wait_obj - unique tag to backup operation
 * @return - 0 on success and -1 on failure
 */
int c_fam_backup_wait(c_fam* fam_obj, void* wait_obj);

/* Restore backup data from  archival storage to data item.
 * @pararm fam_obj - FAM instance
 * @param backup_name - Name of the backup
 * @param dest_desc - Data item in which backup content will be restored.
 * @return - a pointer to the wait object for the restore operation.
 */
void* c_fam_restore(c_fam* fam_obj, const char* backup_name, c_fam_desc* dest_desc);

/* Creates data item, Restore backup data from  archival storage to newly
 * created data item.
 * @param fam_obj - FAM instance
 * @param backup_name - Name of the backup
 * @param dest_reg_desc - Region where data item is to be created for restoring
 * backup content.
 * @param dataitem_name - Data item name in which backup content will be
 * restored.
 * @param perm - Access permissions associated with data item.
 * @param dest_desc - dataitem handle created during fam_restore call.
 * @return - a pointer to the wait object for the restore operation.
 */
void* c_fam_restore_region(c_fam* fam_obj, const char* backup_name, c_fam_region_desc* dest_reg_desc, const char* dataitem_name, mode_t perm, c_fam_desc** dest_desc);

/**
 * Wait for restore operation corresponding to the wait object passed to
 * complete
 * @param fam_obj - FAM instance
 * @param waitObj - unique tag to restore operation
 * @return - 0 on success and -1 on failure
 */
int c_fam_restore_wait(c_fam* fam_obj, void* wait_obj);

/* Deletes the backup mentioned by BackupName.
 * @param fam_obj - FAM instance
 * @param backup_name -  name of backup to be deleted.
 * @return - a pointer to the wait object for the backup deletion operation.
 */
void* c_fam_delete_backup(c_fam* fam_obj, const char* backup_name);

/**
 * Wait for backup deletion operation corresponding to the wait object
 * passed to complete
 * @param fam_obj - FAM instance 
 * @param wait_obj - unique tag to restore operation
 * @return - 0 on success and -1 on failure
 */
int c_fam_delete_backup_wait(c_fam* fam_obj, void* wait_obj);

/**
 * Deallocate allocated space in memory
 * @param fam_obj - FAM instance
 * @param desc - descriptor associated with the space.
 * @return - 0 on success and -1 on failure
 * @see #c_fam_allocate()
 */
int  c_fam_deallocate(c_fam* fam_obj, c_fam_desc* desc);

/**
 * Initiate a copy of data from local memory to FAM, returning before copy
 * is complete
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory. Must point to valid data in local
 * memory
 * @param desc - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor
 * to where data should be copied
 * @param size - number of bytes to be copied from local to FAM
 * @return - 0 on success and -1 on failure
 */
int  c_fam_put_nonblocking(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t offset, size_t size);

/**
 * Initiate a copy of data from FAM to node local memory. Do not wait until
 * copy is finished
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to area in FAM.
 * @param local_addr - pointer to local memory region where data needs to be
 * copied. Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor
 * from where memory should be copied
 * @param size - number of bytes to be copied from global to local memory
 * @return - 0 on success and -1 on failure
 */
int  c_fam_get_nonblocking(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t offset, size_t size);

/**
 * Copy data from local memory to FAM, blocking until the copy is complete.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory. Must point to valid data in local
 * memory
 * @param desc - valid descriptor in FAM
 * @param offset - byte offset within the region defined by the descriptor
 * to where data should be copied
 * @param size - number of bytes to be copied from local to FAM
 * @return - 0 on success and -1 on failure
 */
int  c_fam_put_blocking(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t offset, size_t size);

/**
 * Copy data from FAM to node local memory, blocking the caller while the
 * copy is completed.
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to area in FAM.
 * @param local_addr - pointer to local memory region where data needs to be
 * copied. Must be of appropriate size
 * @param offset - byte offset within the space defined by the descriptor
 * from where memory should be copied
 * @param size - number of bytes to be copied from global to local memory
 * @return - 0 on success and -1 on failure
 */
int  c_fam_get_blocking(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t offset, size_t size);

/**
 * Gather data from FAM to local memory, blocking while the copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array
 * in local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from
 * multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory array. Must be large enough to
 * contain returned data
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be gathered in local memory
 * @param first_element - first element in FAM to include in the strided
 * access
 * @param stride - stride in elements
 * @param element_size - size of the element in bytes
 * @return - 0 on success and -1 on failure
 * @see #c_fam_scatter_strided
 */
int  c_fam_gather_blocking_stride(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t first_element, uint64_t stride, uint64_t element_size);

/**
 * Initiate a gather of data from FAM to local memory, return before
 * completion Gathers disjoint elements within a data item in FAM to a
 * contiguous array in local memory. Currently constrained to gather data
 * from a single FAM descriptor, but can be extended if data needs to be
 * gathered from multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory array. Must be large enough to
 * contain returned data
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be gathered in local memory
 * @param first_element - first element in FAM to include in the strided
 * access
 * @param stride - stride in elements
 * @param element_size - size of the element in bytes
 * @return - 0 on success and -1 on failure 
 */
int  c_fam_gather_nonblocking_stride(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t first_element, uint64_t stride, uint64_t element_size);

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array
 * in local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from
 * multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory array. Must be large enough to
 * contain returned data
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be gathered in local memory
 * @param element_index - array of element indexes in FAM to fetch
 * @param element_size - size of each element in bytes
 * @return - 0 on success and -1 on failure
 * @see #c_fam_scatter_indexed
 */
int  c_fam_gather_blocking_index(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t* element_index, uint64_t element_size);

/**
 * Gather data from FAM to local memory, blocking while copy is complete
 * Gathers disjoint elements within a data item in FAM to a contiguous array
 * in local memory. Currently constrained to gather data from a single FAM
 * descriptor, but can be extended if data needs to be gathered from
 * multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory array. Must be large enough to
 * contain returned data
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be gathered in local memory
 * @param element_index - array of element indexes in FAM to fetch
 * @param element_size - size of each element in bytes
 * @return - 0 on success and -1 on failure
 */
int  c_fam_gather_nonblocking_index(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t* element_index, uint64_t element_size);

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint
 * elements of a data item in FAM. Currently constrained to scatter data to
 * a single FAM descriptor, but can be extended if data needs to be
 * scattered to multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory region containing elements
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be scattered from local memory
 * @param first_element - placement of the first element in FAM to place for
 * the strided access
 * @param stride - stride in elements
 * @param element_size - size of each element in bytes
 * @return - 0 on success and -1 on failure
 */
int  c_fam_scatter_blocking_stride(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t first_element, uint64_t stride, uint64_t element_size);

/**
 * initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint
 * elements of a data item in FAM. Currently constrained to scatter data to
 * a single FAM descriptor, but can be extended if data needs to be
 * scattered to multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory region containing elements
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be scattered from local memory
 * @param first_element - placement of the first element in FAM to place for
 * the strided access
 * @param stride - stride in elements
 * @param element_size - size of each element in bytes
 * @return - 0 on success and -1 on failure
 */
int  c_fam_scatter_nonblocking_stride(c_fam* _fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t first_element, uint64_t stride, uint64_t element_size);

/**
 * Scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint
 * elements of a data item in FAM. Currently constrained to scatter data to
 * a single FAM descriptor, but can be extended if data needs to be
 * scattered to multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory region containing data elements
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be scattered from local memory
 * @param element_index - array containing element indexes
 * @param element_size - size of the element in bytes
 * @return - 0 on success and -1 on failure
 */
int  c_fam_scatter_blocking_index(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t* element_index, uint64_t element_size);

/**
 * Initiate a scatter data from local memory to FAM.
 * Scatters data from a contiguous array in local memory to disjoint
 * elements of a data item in FAM. Currently constrained to scatter data to
 * a single FAM descriptor, but can be extended if data needs to be
 * scattered to multiple data items.
 * @param fam_obj - FAM instance
 * @param local_addr - pointer to local memory region containing data elements
 * @param desc - valid descriptor containing FAM reference
 * @param n_elements - number of elements to be scattered from local memory
 * @param element_index - array containing element indexes
 * @param element_size - size of the element in bytes
 * @return - 0 on success and -1 on failure
 */
int  c_fam_scatter_nonblocking_index(c_fam* fam_obj, void* local_addr, c_fam_desc* desc, uint64_t n_elements, uint64_t* element_index, uint64_t element_size);

/**
 * Map a data item in FAM to the local virtual address space, and return its
 * pointer.
 * @param fam_obj - FAM instance
 * @param desc - descriptor to be mapped
 * @return pointer within the process virtual address space that can be used
 * to directly access the data item in FAM
 * @see #c_fam_unmap()
 */
void* c_fam_map(c_fam* fam_obj, c_fam_desc* desc);

/**
 * Unmap a data item in FAM from the local virtual address space.
 * @param fam_obj - FAM instance
 * @param local_ptr - pointer within the process virtual address space to be
 * unmapped
 * @param desc - descriptor for the FAM to be unmapped
 * @return - 0 on success and -1 on failure
 * @see #c_fam_map()
 */
int c_fam_unmap(c_fam* fam_obj, void* local_ptr, c_fam_desc* desc);

/**
 * fam_quiet - blocks the calling PE thread until all its pending FAM
 * operations (put, scatter, atomics, copy) are completed. Note that method
 * this does NOT order or wait for completion of load/store accesses by the
 * processor to FAM enabled by fam_map().
 * @param fam_obj - FAM instance
 * @return - 0 on success and -1 on failure
 */
int  c_fam_quiet(c_fam* fam_obj);

/**
 * fam_fence - ensures that FAM operations (put, scatter, atomics, copy)
 * issued by the calling PE thread before the fence are ordered before FAM
 * operations issued by the calling thread after the fence Note that method
 * this does NOT order load/store accesses by the processor to FAM enabled
 * by fam_map().
 * @param fam_obj - FAM instance
 * @return - 0 on success and -1 on failure
 */
int  c_fam_fence(c_fam* fam_obj);

/*
 * c_fam_delete - delete the fam instance
 * @param fam_obj - FAM instance
 * @return - none
 */
void c_fam_delete(c_fam* fam_obj);

/**
 * Forcibly terminate all PEs in the same group as the caller
 * @param fam_obj - FAM instance
 * @param status - termination status to be returned by the program.
 * @return - 0 on success and -1 on failure
 */
int c_fam_abort(c_fam* fam_obj, int status);

/**
 * List known options for this version of the library. Provides a way for
 * programs to check which options are known to the library.
 * @param fam_obj - FAM instance
 * @return - an array of character strings containing names of options known
 * to the library
 */
const char ** c_fam_list_options(c_fam* fam_obj);

/**
 * Query the FAM library for an option.
 * @param fam_obj - FAM instance
 * @param option_name - char string containing the name of the option
 * @return pointer to the (read-only) value of the option
 */
const void * c_fam_get_option(c_fam* fam_obj, const char *option_name);

/**
 * Get the size, permissions and name of the dataitem  associated
 * with a data item descriptor.
 * @param fam_obj - FAM instance
 * @param desc - descriptor associated with some dataitem.
 * @param stat - structure that holds aforementioned info.
 * @return - 0 on success and -1 on failure
 */
int c_fam_stat_data(c_fam* fam_obj, c_fam_desc* desc, c_fam_stat* stat);

/**
 * Get the size, permissions and name of the region associated
 * with a region descriptor.
 * Get the size of the region associated with a region descriptor.
 * @param fam_obj - FAM instance
 * @param desc - descriptor associated with some region
 * @param stat - structure that holds aforementioned info.
 * @return - 0 on success and -1 on failure
 */
int c_fam_stat_region(c_fam* fam_obj, c_fam_region_desc* region_desc, c_fam_stat* stat);

/**
 * Resize space allocated to a previously created region.
 * Note that shrinking the size of a region may make descriptors to data
 * items within that region invalid. Thus the method should be used with
 * caution.
 * @param fam_obj - FAM instance
 * @param region_desc - descriptor associated with the previously created
 * region
 * @param size - new requested size of the allocated space
 * @return - 0 on success and -1 on failure
 * @see #c_fam_create_region
 * @see #c_fam_destroy_region
 */
int c_fam_resize_region(c_fam* fam_obj, c_fam_region_desc* region_desc, uint64_t size);

/**
 * Change permissions associated with a data item descriptor.
 * @param fam_obj - FAM instance
 * @param desc - descriptor associated with some data item
 * @param perm - new permissions for the data item
 * @return - 0 on success and -1 on failure
 */
int c_fam_change_permissions_data(c_fam* fam_obj, c_fam_desc* desc, mode_t perm);

/**
 * Change permissions associated with a region descriptor.
 * @param fam_obj - FAM instance
 * @param region_desc - descriptor associated with some region
 * @param perm - new permissions for the region
 * @return - 0 on success and -1 on failure
 */
int c_fam_change_permissions_region(c_fam* fam_obj, c_fam_region_desc* region_desc, mode_t perm);

c_fam_context* c_fam_context_open(c_fam* fam_obj);
int c_fam_context_close(c_fam* fam_obj, c_fam_context * fam_cntxt);

/**
 * fam_progress - returns number of pending FAM
 * operations (put, get, scatter, gather, atomics).
 * @param fam_obj - FAM instance
 * @param value - Pointer to number of pending operations
 * @return - 0 on success and -1 on failure
 */
int c_fam_progress(c_fam* fam_obj, uint64_t* value);

/**
 * retrieve the FAM exception messages when an error occurs.
 * @return - either the error number or pointer to error string.
 */
int c_fam_error(void);
const char* c_fam_error_msg(void);

//Atomic API support

/**
 * set group - atomically set a value at a given offset within a data item
 * in FAM
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be set at the given location
 * @return - 0 on success and -1 on failure
 */
int c_fam_set_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value);
int c_fam_set_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value);
int c_fam_set_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_set_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);
int c_fam_set_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value);
int c_fam_set_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value);
int c_fam_set_int128(c_fam* fam_obj, c_fam_desc* desc , uint64_t offset, int128_t value);

/**
 * fetch group - atomically fetches the value at the given offset within a
 * data item from FAM
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - pointer to return value
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t* value);
int c_fam_fetch_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t* value);
int c_fam_fetch_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t* value);
int c_fam_fetch_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t* value);
int c_fam_fetch_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float* value);
int c_fam_fetch_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double* value);
int c_fam_fetch_int128(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int128_t* value);

/**
 * add group - atomically add a value to the value at a given offset within
 * a data item in FAM
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be added to the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_add_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value);
int c_fam_add_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value);
int c_fam_add_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_add_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);
int c_fam_add_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value);
int c_fam_add_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value);

/**
 * subtract group - atomically subtract a value from a value at a given
 * offset within a data item in FAM
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be subtracted from the existing value at the
 * given location
 * @return - 0 on success and -1 on failure
 */
int c_fam_subtract_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value);
int c_fam_subtract_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value);
int c_fam_subtract_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_subtract_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);
int c_fam_subtract_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value);
int c_fam_subtract_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value);

/**
 * min group - atomically set the value at a given offset within a data item
 * in FAM to the smaller of the existing value and the given value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be compared to the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_min_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value);
int c_fam_min_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value);
int c_fam_min_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_min_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);
int c_fam_min_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value);
int c_fam_min_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value);

/**
 * max group - atomically set the value at a given offset within a data item
 * in FAM to the larger of the value and the given value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be compared to the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_max_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value);
int c_fam_max_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value);
int c_fam_max_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_max_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);
int c_fam_max_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value);
int c_fam_max_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value);

/**
 * and group - atomically replace the value at a given offset within a data
 * item in FAM with the logical AND of that value and the given value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_and_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_and_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);

/**
 * or group - atomically replace the value at a given offset within a data
 * item in FAM with the logical OR of that value and the given value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_or_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_or_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);

/**
 * xor group - atomically replace the value at a given offset within a data
 * item in FAM with the logical XOR of that value and the given value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @return - 0 on success and -1 on failure
 */
int c_fam_xor_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value);
int c_fam_xor_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value);

/**
 * swap group - atomically replace the value at the given offset within a
 * data item in FAM with the given value, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be swapped with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_swap_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value, int32_t* ret_val);
int c_fam_swap_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value, int64_t* ret_val);
int c_fam_swap_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_swap_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);
int c_fam_swap_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value, float* ret_val);
int c_fam_swap_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value , double* ret_val);

/**
 * compare and swap group - atomically conditionally replace the value at
 * the given offset within a data item in FAM with the given value, and
 * return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param old_val - value to be compared with the existing value at the
 * given location
 * @param new_val - new value to be stored if comparison is successful
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_compare_swap_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t old_val, int32_t new_val, int32_t* ret_val);
int c_fam_compare_swap_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t old_val, int64_t new_val, int64_t* ret_val);
int c_fam_compare_swap_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t old_val, uint32_t new_val, uint32_t* ret_val);
int c_fam_compare_swap_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t old_val, uint64_t new_val, uint64_t* ret_val);
int c_fam_compare_swap_int128(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int128_t old_val, int128_t new_val, int128_t* ret_val);

/**
 * fetch and add group - atomically add the given value to the value at the
 * given offset within a data item in FAM, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be added to the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_add_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value, int32_t* ret_val);
int c_fam_fetch_add_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value, int64_t* ret_val);
int c_fam_fetch_add_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_add_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);
int c_fam_fetch_add_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value, float* ret_val);
int c_fam_fetch_add_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value, double* ret_val);

/**
 * fetch and subtract group - atomically subtract the given value from the
 * value at the given offset within a data item in FAM, and return the old
 * value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be subtracted from the existing value at the
 * given location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_subtract_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value, int32_t* ret_val);
int c_fam_fetch_subtract_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value, int64_t* ret_val);
int c_fam_fetch_subtract_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_subtract_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);
int c_fam_fetch_subtract_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value, float* ret_val);
int c_fam_fetch_subtract_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value, double* ret_val);

/**
 * fetch and min group - atomically set the value at a given offset within a
 * data item in FAM to the smaller of the existing value and the given
 * value, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_min_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value, int32_t* ret_val);
int c_fam_fetch_min_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value, int64_t* ret_val);
int c_fam_fetch_min_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_min_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);
int c_fam_fetch_min_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value, float* ret_val);
int c_fam_fetch_min_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value, double* ret_val);

/**
 * fetch and max group - atomically set the value at a given offset within a
 * data item in FAM to the larger of the existing value and the given value,
 * and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be compared with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_max_int32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int32_t value, int32_t* ret_val);
int c_fam_fetch_max_int64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, int64_t value, int64_t* ret_val);
int c_fam_fetch_max_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_max_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);
int c_fam_fetch_max_float(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, float value, float* ret_val);
int c_fam_fetch_max_double(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, double value, double* ret_val);

/**
 * fetch and and group - atomically replace the value at a given offset
 * within a data item in FAM with the logical AND of that value and the
 * given value, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_and_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value , uint32_t* ret_val);
int c_fam_fetch_and_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);

/**
 * fetch and or group - atomically replace the value at a given offset
 * within a data item in FAM with the logical OR of that value and the given
 * value, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_or_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_or_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);

/**
 * fetch and xor group - atomically replace the value at a given offset
 * within a data item in FAM with the logical XOR of that value and the
 * given value, and return the old value
 * @param fam_obj - FAM instance
 * @param desc - valid descriptor to data item in FAM
 * @param offset - byte offset within the data item of the value to be
 * updated
 * @param value - value to be combined with the existing value at the given
 * location
 * @param ret_val - pointer to old value from the given location in FAM
 * @return - 0 on success and -1 on failure
 */
int c_fam_fetch_xor_uint32(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint32_t value, uint32_t* ret_val);
int c_fam_fetch_xor_uint64(c_fam* fam_obj, c_fam_desc* desc, uint64_t offset, uint64_t value, uint64_t* ret_val);

#ifdef __cplusplus
} //end of extern "C" 
#endif

#endif// FAM_C_H_

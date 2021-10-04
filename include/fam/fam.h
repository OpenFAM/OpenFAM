/*
 * fam.h
 * Copyright (c) 2017, 2018, 2020, 2021 Hewlett Packard Enterprise Development,
 * LP. All rights reserved. Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided that the following
 * conditions are met:
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
 * Created Oct 22, 2017, by Sharad Singhal
 * Modified Nov 1, 2017, by Sharad Singhal, added C API
 * Modified Nov 2, 2017, by Sharad Singhal based on discussion with Kim Keeton
 * Modified Nov 3, 2017, by Kim Keeton based on discussion with Sharad Singhal
 * Modified Dec 10, 2017, by Sharad Singhal to C11. Initial distribution for
 * comments Modified Dec 13, 2017, by Sharad Singhal to update to OpenFAM-API
 * version 1.01 Modified Feb 20, 2018, by Sharad Singhal to update to
 * OpenFAM-API version 1.02 Modified Feb 23, 2018, by Kim Keeton to update to
 * OpenFAM-API version 1.03 Modified Apr 24, 2018, by Sharad Singhal to update
 * to OpenFAM-API version 1.04 Modified Oct 5, 2018, by Sharad Singhal to
 * include C++ definitions Modifed Oct 22, 2018 by Sharad Singhal to separate
 * out C and C11 definitions
 * Modified July 14, 2020, by Faizan Barmawer to add fam_stat API definition,
 * change signature of fam_initialize, fam_resize_region, fam_copy,
 * fam*_blocking APIs, fam_change_permissions and removed fam_size API
 * definition
 *
 * Work in progress, UNSTABLE
 * Uses _Generic and 128-bit integer types, tested under gcc 6.3.0. May require
 * “-std=c11” compiler flag if you are using the generic API as documented in
 * OpenFAM-API-v104.
 *
 * Programming conventions used in the API:
 * APIs are defined using underscore separated words. Types start with initial
 * capitals, while names of function calls start with lowercase letters.
 * Variable and parameter names are camelCase with lower case initial letter.
 * All APIs have the prefix "Fam_" or "fam_" depending on whether the name
 * represents a type or a function.
 *
 * Where multiple methods representing same function with different data types
 * in the signature exist, generics are used to map the functions to the same
 * name.
 *
 * Where different types are involved (e.g. in atomics) and the method signature
 * is not sufficient to separate out the function calls, we follow the SHMEM
 * convention of adding _TYPE as a suffix for the C API.
 *
 */
#ifndef FAM_H_
#define FAM_H_

#include <stdint.h>   // needed for uint64_t etc.
#include <sys/stat.h> // needed for mode_t

#ifdef __cplusplus
/** C++ Header
 *  The header is defined as a single interface containing all desired methods
 */
namespace openfam {

#define FAM_SUCCESS 0

/**
 * Currently support for 128-bit integers is spotty in GCC since the C standard
 * does not appear to define it. The following will use __int128 if defined in
 * gcc, else change 128-bit declarations to "long long" May need to be changed
 * based on compiler support.
 * Also, in some machines macro __int128 is not defined, We can check for
 * defenition of __SIZEOF_INT128__ in those machines. Based on
 * https://stackoverflow.com/questions/16088282/is-there-a-128-bit-integer-in-gcc
 *
 */
#ifndef int128_t
#ifdef __int128
typedef __int128 int128_t;
#else
#ifdef __SIZEOF_INT128__
typedef __int128 int128_t;
#else
typedef long long int128_t;
#endif // __SIZEOF_INT128__
#endif // __int128t
#endif // int128_t

/**
 * Enumeration defining interleaving options for FAM.
 */
typedef enum { INTERLEAVE_DEFAULT = 0, ENABLE, DISABLE } Fam_Interleave_Enable;

/**
 * Enumeration defining memory types supported in FAM.
 */
typedef enum { MEMORY_TYPE_DEFAULT = 0, VOLATILE, PERSISTANT } Fam_Memory_Type;

/**
 * Enumeration defining redundancy options for FAM. This enum defines redundancy
 * levels (software or hardware) supported within the library.
 */
typedef enum {
    /** System Default Value */
    REDUNDANCY_LEVEL_DEFAULT = 0,
    /** No redundancy is provided */
    NONE,
    /** RAID 1 equivalent redundancy is provided */
    RAID1,
    /** RAID 5 equivalent redundancy is provided */
    RAID5
} Fam_Redundancy_Level;

typedef struct {
    Fam_Redundancy_Level redundancyLevel;
    Fam_Memory_Type memoryType;
    Fam_Interleave_Enable interleaveEnable;
} Fam_Region_Attributes;
/**
 * Enumeration defining descriptor update on several  fields.
 */
typedef enum {
    /** Descriptor is invalid */
    DESC_INVALID,
    /** Descriptor is initialized*/
    DESC_INIT_DONE,
    /** Descriptor is initialized, but valid key isnt present*/
    DESC_INIT_DONE_BUT_KEY_NOT_VALID,
    /** Descriptor is uninitialized*/
    DESC_UNINITIALIZED
} Fam_Descriptor_Status;

/**
 * FAM Global descriptor represents both the region and data item in FAM.
 */
typedef struct {
    /** region ID for this descriptor */
    uint64_t regionId;
    /*
     * Data Item : Offset within the region for the start of the memory
     * represented the descriptor Region    : FAM_REGION_OFFSET(magic number)
     */
    uint64_t offset;
} Fam_Global_Descriptor;

typedef struct {
    uint64_t key;
    uint64_t size;
    mode_t perm;
    char *name;
} Fam_Stat;
/**
 * Structure defining a FAM descriptor. Descriptors are PE independent data
 * structures that enable the OpenFAM library to uniquely locate an area of
 * fabric attached memory. All fields within this data structure are reserved
 * for use within OpenFAM library implementations, and application code should
 * not rely on the presence of any field within this data structure.
 * Applications should treat descriptors as opaque read-only data structures.
 */
class Fam_Descriptor {
  public:
    // Constructor
    Fam_Descriptor(Fam_Global_Descriptor gDescriptor, uint64_t itemSize);
    Fam_Descriptor(Fam_Global_Descriptor gDescriptor);
    Fam_Descriptor();
    // Destructor
    ~Fam_Descriptor();
    // return Global descriptor
    Fam_Global_Descriptor get_global_descriptor();
    // bind key.
    void bind_key(uint64_t tempKey);
    // get keys
    uint64_t get_key();
    // get context
    void *get_context();
    // set context
    void set_context(void *context);
    void set_base_address(void *address);
    void *get_base_address();
    // get status
    int get_desc_status();
    // put status
    void set_desc_status(int update_status);
    // set size, perm and name.
    void set_size(uint64_t itemSize);
    void set_perm(mode_t perm);
    void set_name(char *name);
    // get size, perm and name.
    uint64_t get_size();
    mode_t get_perm();
    char *get_name();
    // get memory server id
    uint64_t get_memserver_id();

  private:
    class FamDescriptorImpl_;
    FamDescriptorImpl_ *fdimpl_;
};

/**
 * Descriptor defining a large region of memory, within which other data
 * structures can reside. A region descriptor maintains a pointer (descriptor)
 * to the overall region, as well as associated redundancy information for that
 * region. Note that a region may have overall access permissions associated
 * with it, which are combined with individual permissions defined for other
 * descriptors within it to manage access control.
 */
class Fam_Region_Descriptor {
    // Constructor
  public:
    Fam_Region_Descriptor(Fam_Global_Descriptor gDescriptor,
                          uint64_t regionSize);
    Fam_Region_Descriptor(Fam_Global_Descriptor gDescriptor);
    Fam_Region_Descriptor();
    // Destructor
    ~Fam_Region_Descriptor();
    // return Global descriptor
    Fam_Global_Descriptor get_global_descriptor();
    // get context
    void *get_context();
    // set context
    void set_context(void *context);
    // get status
    int get_desc_status();
    // put status
    void set_desc_status(int update_status);
    // set size, perm and name.
    void set_size(uint64_t itemSize);
    void set_perm(mode_t perm);
    void set_name(char *name);
    void set_redundancyLevel(Fam_Redundancy_Level redundancyLevel);
    void set_memoryType(Fam_Memory_Type memoryType);
    void set_interleaveEnable(Fam_Interleave_Enable interleaveEnable);
    Fam_Redundancy_Level get_redundancyLevel();
    Fam_Memory_Type get_memoryType();
    Fam_Interleave_Enable get_interleaveEnable();
    // get size, perm and name.
    uint64_t get_size();
    mode_t get_perm();
    char *get_name();
    // get memory server id
    uint64_t get_memserver_id();

  private:
    class FamRegionDescriptorImpl_;
    FamRegionDescriptorImpl_ *frdimpl_;
};

/**
 * Structure defining FAM options. This structure holds system wide information
 * required to initialize the OpenFAM library and the associated program using
 * the library. It is expected to evolve over time as the library is
 * implemented. Currently defined options are included below.
 */
typedef struct {
    /** Default region to be used within the program */
    char *defaultRegionName;
    /** CIS servers to be used by OpenFam PEs */
    char *cisServer;
    /** Port to be used by OpenFam for CIS server RPC connection */
    char *grpcPort;
    /** Libfabric provider to be used by OpenFam libfabric datapath operations;
     * "sockets" by default */
    char *libfabricProvider;
    /** FAM thread model */
    char *famThreadModel;
    /** CIS interface to be used, Default is RPC, Supports Direct calls too */
    char *cisInterfaceType;
    /** OpenFAM model to be used; default is memory_server, Other option is
     * shared_memory */
    char *openFamModel;
    /** FAM context model - Default, Region*/
    char *famContextModel;
    /** Number of consumer threads for shared memory model **/
    char *numConsumer;
    /** FAM runtime - Default, pmix*/
    char *runtime;
} Fam_Options;

class fam {
  public:
    // INITIALIZE group
    /**
     * Initialize the OpenFAM library. This method is required to be the first
     * method called when a process uses the OpenFAM library.
     * @param groupName - name of the group of cooperating PEs.
     * @param options - options structure containing initialization choices
     * @return - none
     * @see #fam_finalize()
     * @see #Fam_Options
     */
    void fam_initialize(const char *groupName, Fam_Options *options);

    /**
     * Finalize the fam library. Once finalized, the process can continue work,
     * but it is disconnected from the OpenFAM library functions.
     * @param groupName - name of group of cooperating PEs for this job
     * @return - none
     * @see #fam_initialize()
     */
    void fam_finalize(const char *groupName);

    /**
     * Forcibly terminate all PEs in the same group as the caller
     * @param status - termination status to be returned by the program.
     * @return - none
     */
    void fam_abort(int status);

    /**
     * Suspends the execution of the calling PE until all other PEs issue
     * a call to this particular fam_barrier_all() statement
     * @return - none
     */
    void fam_barrier_all();

    /**
     * List known options for this version of the library. Provides a way for
     * programs to check which options are known to the library.
     * @return - an array of character strings containing names of options known
     * to the library
     * @see #fam_get_option()
     */
    const char **fam_list_options(void);

    /**
     * Query the FAM library for an option.
     * @param optionName - char string containing the name of the option
     * @return pointer to the (read-only) value of the option
     * @see #fam_list_options()
     */
    const void *fam_get_option(char *optionName);

    /**
     * Look up a region in FAM by name in the name service.
     * @param name - name of the region.
     * @return - The descriptor to the region. Null if no such region exists, or
     * if the caller does not have access.
     * @see #fam_lookup
     */
    Fam_Region_Descriptor *fam_lookup_region(const char *name);

    /**
     * look up a data item in FAM by name in the name service.
     * @param itemName - name of the data item
     * @param regionName - name of the region containing the data item
     * @return descriptor to the data item if found. Null if no such data item
     * is registered, or if the caller does not have access.
     * @see #fam_lookup_region
     */
    Fam_Descriptor *fam_lookup(const char *itemName, const char *regionName);

    // ALLOCATION Group

    /**
     * Allocate a large region of FAM. Regions are primarily used as large
     * containers within which additional memory may be allocated and managed by
     * the program. The API is extensible to support additional (implementation
     * dependent) options. Regions are long-lived and are automatically
     * registered with the name service.
     * @param name - name of the region
     * @param size - size (in bytes) requested for the region. Note that
     * implementations may round up the size to implementation-dependent sizes,
     * and may impose system-wide (or user-dependent) limits on individual and
     * total size allocated to a given user.
     * @param permissions - access permissions to be used for the region
     * @param redundancyLevel - desired redundancy level for the region
     * @return - Region_Descriptor for the created region
     * @see #fam_resize_region
     * @see #fam_destroy_region
     */
    Fam_Region_Descriptor *
    fam_create_region(const char *name, uint64_t size, mode_t permissions,
                      Fam_Region_Attributes *regionAttributes);

    /**
     * Destroy a region, and all contents within the region. Note that this
     * method call will trigger a delayed free operation to permit other
     * instances currently using the region to finish.
     * @param descriptor - descriptor for the region
     * @return - none
     * @see #fam_create_region
     * @see #fam_resize_region
     */
    void fam_destroy_region(Fam_Region_Descriptor *descriptor);

    /**
     * Resize space allocated to a previously created region.
     * Note that shrinking the size of a region may make descriptors to data
     * items within that region invalid. Thus the method should be used with
     * caution.
     * @param descriptor - descriptor associated with the previously created
     * region
     * @param nbytes - new requested size of the allocated space
     * @return - none
     * @see #fam_create_region
     * @see #fam_destroy_region
     */
    void fam_resize_region(Fam_Region_Descriptor *descriptor, uint64_t nbytes);

    /**
     * Allocate some unnamed space within a region. Allocates an area of FAM
     * within a region
     * @param name - (optional) name of the data item
     * @param nybtes - size of the space to allocate in bytes.
     * @param accessPermissions - permissions associated with this space
     * @param region - descriptor of the region within which the space is being
     * allocated. If not present or null, a default region is used.
     * @return - descriptor that can be used within the program to refer to this
     * space
     * @see #fam_deallocate()
     */
    Fam_Descriptor *fam_allocate(uint64_t nbytes, mode_t accessPermissions,
                                 Fam_Region_Descriptor *region);
    Fam_Descriptor *fam_allocate(const char *name, uint64_t nbytes,
                                 mode_t accessPermissions,
                                 Fam_Region_Descriptor *region);

    /**
     * Deallocate allocated space in memory
     * @param descriptor - descriptor associated with the space.
     * @return - none
     * @see #fam_allocate()
     */
    void fam_deallocate(Fam_Descriptor *descriptor);

    /**
     * Change permissions associated with a data item descriptor.
     * @param descriptor - descriptor associated with some data item
     * @param accessPermissions - new permissions for the data item
     * @return - none
     */
    void fam_change_permissions(Fam_Descriptor *descriptor,
                                mode_t accessPermissions);

    /**
     * Change permissions associated with a region descriptor.
     * @param descriptor - descriptor associated with some region
     * @param accessPermissions - new permissions for the region
     * @return - none
     */
    void fam_change_permissions(Fam_Region_Descriptor *descriptor,
                                mode_t accessPermissions);

    /**
     * Get the size, permissions and name of the dataitem  associated
     * with a data item descriptor.
     * @param descriptor - descriptor associated with some dataitem.
     * @param famInfo - structure that holds aforementioned info.
     * @return - none
     */
    void fam_stat(Fam_Descriptor *descriptor, Fam_Stat *famInfo);

    /**
     * Get the size, permissions and name of the region associated
     * with a region descriptor.
     * Get the size of the region associated with a region descriptor.
     * @param descriptor - descriptor associated with some region
     * @param famInfo - structure that holds aforementioned info.
     * @return - none
     */
    void fam_stat(Fam_Region_Descriptor *descriptor, Fam_Stat *famInfo);

    // DATA READ AND WRITE Group. These APIs read and write data in FAM and copy
    // data between local DRAM and FAM.

    // DATA GET/PUT sub-group

    /**
     * Copy data from FAM to node local memory, blocking the caller while the
     * copy is completed.
     * @param descriptor - valid descriptor to area in FAM.
     * @param local - pointer to local memory region where data needs to be
     * copied. Must be of appropriate size
     * @param offset - byte offset within the space defined by the descriptor
     * from where memory should be copied
     * @param nbytes - number of bytes to be copied from global to local memory
     * @return - none
     */
    void fam_get_blocking(void *local, Fam_Descriptor *descriptor,
                          uint64_t offset, uint64_t nbytes);

    /**
     * Initiate a copy of data from FAM to node local memory. Do not wait until
     * copy is finished
     * @param descriptor - valid descriptor to area in FAM.
     * @param local - pointer to local memory region where data needs to be
     * copied. Must be of appropriate size
     * @param offset - byte offset within the space defined by the descriptor
     * from where memory should be copied
     * @param nbytes - number of bytes to be copied from global to local memory
     * @return - none
     */
    void fam_get_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes);

    /**
     * Copy data from local memory to FAM, blocking until the copy is complete.
     * @param local - pointer to local memory. Must point to valid data in local
     * memory
     * @param descriptor - valid descriptor in FAM
     * @param offset - byte offset within the region defined by the descriptor
     * to where data should be copied
     * @param nbytes - number of bytes to be copied from local to FAM
     * @return - none
     */
    void fam_put_blocking(void *local, Fam_Descriptor *descriptor,
                          uint64_t offset, uint64_t nbytes);

    /**
     * Initiate a copy of data from local memory to FAM, returning before copy
     * is complete
     * @param local - pointer to local memory. Must point to valid data in local
     * memory
     * @param descriptor - valid descriptor in FAM
     * @param offset - byte offset within the region defined by the descriptor
     * to where data should be copied
     * @param nbytes - number of bytes to be copied from local to FAM
     * @return - none
     */
    void fam_put_nonblocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t offset, uint64_t nbytes);

    // LOAD/STORE sub-group

    /**
     * Map a data item in FAM to the local virtual address space, and return its
     * pointer.
     * @param descriptor - descriptor to be mapped
     * @return pointer within the process virtual address space that can be used
     * to directly access the data item in FAM
     * @see #fm_unmap()
     */
    void *fam_map(Fam_Descriptor *descriptor);

    /**
     * Unmap a data item in FAM from the local virtual address space.
     * @param local - pointer within the process virtual address space to be
     * unmapped
     * @param descriptor - descriptor for the FAM to be unmapped
     * @return - none
     * @see #fam_map()
     */
    void fam_unmap(void *local, Fam_Descriptor *descriptor);

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
     * @return - none
     * @see #fam_scatter_strided
     */
    void fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t firstElement,
                             uint64_t stride, uint64_t elementSize);

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
     * @return - none
     * @see #fam_scatter_indexed
     */
    void fam_gather_blocking(void *local, Fam_Descriptor *descriptor,
                             uint64_t nElements, uint64_t *elementIndex,
                             uint64_t elementSize);

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
     * @return - none
     * @see #fam_scatter_strided
     */
    void fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t firstElement,
                                uint64_t stride, uint64_t elementSize);

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
     * @return - none
     * @see #fam_scatter_indexed
     */
    void fam_gather_nonblocking(void *local, Fam_Descriptor *descriptor,
                                uint64_t nElements, uint64_t *elementIndex,
                                uint64_t elementSize);

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
     * @return - none
     * @see #fam_gather_strided
     */
    void fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t nElements, uint64_t firstElement,
                              uint64_t stride, uint64_t elementSize);

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
     * @return - none
     * @see #fam_gather_indexed
     */
    void fam_scatter_blocking(void *local, Fam_Descriptor *descriptor,
                              uint64_t nElements, uint64_t *elementIndex,
                              uint64_t elementSize);

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
     * @return - none
     * @see #fam_gather_strided
     */
    void fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t firstElement,
                                 uint64_t stride, uint64_t elementSize);

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
     * @return - none
     * @see #fam_gather_indexed
     */
    void fam_scatter_nonblocking(void *local, Fam_Descriptor *descriptor,
                                 uint64_t nElements, uint64_t *elementIndex,
                                 uint64_t elementSize);

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
     * @return - a pointer to the wait object for the copy operation
     * @see #fam_copy_wait
     */
    void *fam_copy(Fam_Descriptor *src, uint64_t srcOffset,
                   Fam_Descriptor *dest, uint64_t destOffset, uint64_t nbytes);

    /**
     * Wait for copy operation correspond to the wait object passed to complete
     * @param waitObj - unique tag to copy operation
     * @return - none
     */
    void fam_copy_wait(void *waitObj);

    // Backup data item to a specific file
    void *fam_backup(Fam_Descriptor *src, char *outputFile);
    // Restore data item  info from  a specific file to descriptor.
    void *fam_restore(char *inputFile, Fam_Descriptor *dest);
    void *fam_restore(char *inputFile, Fam_Region_Descriptor *destRegion,
                      char *dataitemName, mode_t accessPermissions,
                      Fam_Descriptor **dest);

    void fam_backup_wait(void *waitObj);
    void fam_restore_wait(void *waitObj);

    // ATOMICS Group

    // NON fetching routines

    /**
     * set group - atomically set a value at a given offset within a data item
     * in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be set at the given location
     * @return - none
     */
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, int128_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_set(Fam_Descriptor *descriptor, uint64_t offset, double value);

    /**
     * add group - atomically add a value to the value at a given offset within
     * a data item in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be added to the existing value at the given
     * location
     * @return - none
     */
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_add(Fam_Descriptor *descriptor, uint64_t offset, double value);

    /**
     * subtract group - atomically subtract a value from a value at a given
     * offset within a data item in FAM
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be subtracted from the existing value at the
     * given location
     * @return - none
     */
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      int32_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      int64_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      uint32_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      uint64_t value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                      double value);

    /**
     * min group - atomically set the value at a given offset within a data item
     * in FAM to the smaller of the existing value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared to the existing value at the given
     * location
     * @return - none
     */
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_min(Fam_Descriptor *descriptor, uint64_t offset, double value);

    /**
     * max group - atomically set the value at a given offset within a data item
     * in FAM to the larger of the value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be compared to the existing value at the given
     * location
     * @return - none
     */
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, int32_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, int64_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, float value);
    void fam_max(Fam_Descriptor *descriptor, uint64_t offset, double value);

    /**
     * and group - atomically replace the value at a given offset within a data
     * item in FAM with the logical AND of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - none
     */
    void fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_and(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    /**
     * or group - atomically replace the value at a given offset within a data
     * item in FAM with the logical OR of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - none
     */
    void fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_or(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

    /**
     * xor group - atomically replace the value at a given offset within a data
     * item in FAM with the logical XOR of that value and the given value
     * @param descriptor - valid descriptor to data item in FAM
     * @param offset - byte offset within the data item of the value to be
     * updated
     * @param value - value to be combined with the existing value at the given
     * location
     * @return - none
     */
    void fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint32_t value);
    void fam_xor(Fam_Descriptor *descriptor, uint64_t offset, uint64_t value);

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
    int32_t fam_fetch_int32(Fam_Descriptor *descriptor, uint64_t offset);
    int64_t fam_fetch_int64(Fam_Descriptor *descriptor, uint64_t offset);
    int128_t fam_fetch_int128(Fam_Descriptor *descriptor, uint64_t offset);
    uint32_t fam_fetch_uint32(Fam_Descriptor *descriptor, uint64_t offset);
    uint64_t fam_fetch_uint64(Fam_Descriptor *descriptor, uint64_t offset);
    float fam_fetch_float(Fam_Descriptor *descriptor, uint64_t offset);
    double fam_fetch_double(Fam_Descriptor *descriptor, uint64_t offset);

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
    int32_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                     int32_t value);
    int64_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                     int64_t value);
    uint32_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      uint32_t value);
    uint64_t fam_swap(Fam_Descriptor *descriptor, uint64_t offset,
                      uint64_t value);
    float fam_swap(Fam_Descriptor *descriptor, uint64_t offset, float value);
    double fam_swap(Fam_Descriptor *descriptor, uint64_t offset, double value);

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
    int32_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int32_t oldValue, int32_t newValue);
    int64_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                             int64_t oldValue, int64_t newValue);
    uint32_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint32_t oldValue, uint32_t newValue);
    uint64_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              uint64_t oldValue, uint64_t newValue);
    int128_t fam_compare_swap(Fam_Descriptor *descriptor, uint64_t offset,
                              int128_t oldValue, int128_t newValue);

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
    int32_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_add(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

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
    int32_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                               int32_t value);
    int64_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                               int64_t value);
    uint32_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                uint32_t value);
    uint64_t fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                                uint64_t value);
    float fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                             float value);
    double fam_fetch_subtract(Fam_Descriptor *descriptor, uint64_t offset,
                              double value);

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
    int32_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_min(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

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
    int32_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                          int32_t value);
    int64_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                          int64_t value);
    uint32_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);
    float fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                        float value);
    double fam_fetch_max(Fam_Descriptor *descriptor, uint64_t offset,
                         double value);

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
    uint32_t fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_and(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);

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
    uint32_t fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                          uint32_t value);
    uint64_t fam_fetch_or(Fam_Descriptor *descriptor, uint64_t offset,
                          uint64_t value);

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
    uint32_t fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                           uint32_t value);
    uint64_t fam_fetch_xor(Fam_Descriptor *descriptor, uint64_t offset,
                           uint64_t value);

    // MEMORY ORDERING Routines - provide ordering of FAM operations issued by a
    // PE

    /**
     * fam_fence - ensures that FAM operations (put, scatter, atomics, copy)
     * issued by the calling PE thread before the fence are ordered before FAM
     * operations issued by the calling thread after the fence Note that method
     * this does NOT order load/store accesses by the processor to FAM enabled
     * by fam_map().
     * @return - none
     */
    void fam_fence(void);

    /**
     * fam_quiet - blocks the calling PE thread until all its pending FAM
     * operations (put, scatter, atomics, copy) are completed. Note that method
     * this does NOT order or wait for completion of load/store accesses by the
     * processor to FAM enabled by fam_map().
     * @return - none
     */
    void fam_quiet(void);

    /**
     * fam_progress - returns number of pending FAM
     * operations (put, get, scatter, gather, atomics).
     * @return - number of pending operations
     */
    uint64_t fam_progress(void);

    /**
     * fam() - constructor for fam class
     */
    fam();

    /**
     * ~fam() - destructor for fam class
     */
    ~fam();

  private:
    class Impl_;
    Impl_ *pimpl_;
};
} // namespace openfam

#endif /* end of C/C11 Headers */

#endif /* end of FAM_H_ */

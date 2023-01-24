
OpenFAM Release Notes
---------------------

This file contains user visible OpenFAM changes

v3.0.0
------
 - Added support for volatile and persistent memory types.
 - Multi-threaded support has been added that enables OpenFAM to be used with
   multi-threaded applications.
 - Performance improvements: Data item interleaving feature has been added in
   which a given data item will span across multiple memory servers. This has
   been done to uniformly distribute the load across multiple memory servers,
   thus reducing the network bottlenecks, and thereby improving the overall
   performance. Data item interleaving is enabled by default.
 - Added support for multiple contexts. The data path APIs (get,put, scatter,
   gather and atomics) can be invoked from user defined contexts.
 - OpenFAM APIs changes:
   - fam_create_region API has been modified. Region specific attributes like
     redundancyLevel, etc should now be specified using a structure called
	 Fam_Region_Attributes.
   - Two new APIs have been added to support multiple contexts namely
     fam_context_open() and fam_context_close(). These APIs provide the
	 flexibility to threads to invoke data path operations in parallel.
   - A new API fam_progress has been provided that returns the count of pending
     data path and atomic operations.
   - Few APIs have been added to enable backing up and restoring of data items
     namely - fam_backup() and fam_restore(). An API has been provided to delete
	 the unused backup in the memory server.

v2.0.1
------
 - Fam Option "runtime" was not being fetched from config file. It was using 
   the default value of PMIX.
 - Third party scripts has been modified to use NVMM v0.1. Previously it was 
   using NVMM master branch.
 
v2.0.0
------
 - Scalability improvements: This version supports a larger pool of memory by  
   allowing multiple memory servers to be used. 
 - A memory node contains three new services: 
    - The client interface service
    - metadata management service and 
    - memory management service.
    
 - OpenFAM environment uses four seperate configuration files - three for the  
   memory and metadata services and one for OpenFAM client. These are:  
   - fam_client_interface_config.yaml 
   - fam_memoryserver_config.yaml 
   - fam_metadata_config.yaml 
   - fam_pe_config.yaml 

   All the services and the PE read the respective configuration file from  
   "config" directory placed at the path mentioned by OPENFAM_ROOT environment  
   variable or /opt/OpenFAM.
 - Performance improvements: Memory management and metadata operations have  
   been optimized. 
 - Background worker threads (unused in v0.4.0) were disabled from the  
   memory service.
 - Allocation performance was improved by moving the memset operation to zero   
   allocated memory from fam_allocate() to fam_deallocate() path.
 - Regions can span multiple memory servers, thus reducing network bottlenecks  
   when multiple data items within the same region are used.
 - fam_size has been replaced with fam_stat.
 - fam_copy has been changed to allow data copies between regions, as well as  
   within a region.
 - The return type was changed from int to void for the following APIs:  
   fam_initialize, fam_resize_region, fam_change_permissions, fam_put_blocking,  
   fam_get_blocking, fam_gather_blocking, fam_scatter_blocking.  
 - fam_barrier_all has been added as a supported API.
 - Exception and error handling was simplified. All OpenFAM APIs returns only one  
   exception type on error, i.e, Fam_Exception.
   
v0.4.0
------
 - Added third-party build scripts for centos 7.8.
 - Fixed unregister of data item on destroy region.
 - Fixed hangs seen with multi-threaded tests on data path blocking operations
 - Fixed OpenFAM build failure with latest googletest.

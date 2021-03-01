
OpenFAM Release Notes
---------------------

This file contains user visible OpenFAM changes
 
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

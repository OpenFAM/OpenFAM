#include <iostream>
#include <string.h>

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

int main() {
    fam *my_fam = new fam();
    Fam_Options fam_opts;
    Fam_Region_Descriptor *desc;

    init_fam_options(&fam_opts);
    int *gBuf = (int *)malloc(1024);
    fam_opts.local_buf_addr = gBuf;
    fam_opts.local_buf_size = 1024;

    try {
        my_fam->fam_initialize("default", &fam_opts);
        cout << "FAM initialized successfully" << endl;
    } catch (Fam_Exception &e) {
        cout << "FAM initialization failed: " << e.fam_error_msg() << endl;
        return 1;
    }

    try {
        // Create a region for testing
        desc = my_fam->fam_create_region("test_region", 8192, 0777, NULL);
        if (desc == NULL) {
            cout << "FAM create region failed" << endl;
            return 1;
        }

        // Allocate a new local buffer
        void *local_buffer = malloc(1024);
        if (local_buffer == NULL) {
            cout << "Failed to allocate local buffer" << endl;
            return 1;
        }

        // Register the local buffer
        try {
            my_fam->fam_register_local_buffer(local_buffer, 1024);
            cout << "Local buffer registered successfully" << endl;
        } catch (Fam_Exception &e) {
            cout << "FAM register local buffer failed: " << e.fam_error_msg()
                 << endl;
            free(local_buffer);
            return 1;
        }

        // Deregister the local buffer
        try {
            my_fam->fam_deregister_local_buffer(local_buffer, 1024);
            cout << "Local buffer deregistered successfully" << endl;
        } catch (Fam_Exception &e) {
            cout << "FAM deregister local buffer failed: " << e.fam_error_msg()
                 << endl;
            free(local_buffer);
            return 1;
        }

        free(local_buffer);

        // Destroy the region
        my_fam->fam_destroy_region(desc);
        cout << "FAM region destroyed successfully" << endl;

    } catch (Fam_Exception &e) {
        cout << "Exception caught: " << e.fam_error_msg() << endl;
        return 1;
    }

    try {
        my_fam->fam_finalize("default");
        cout << "FAM finalized successfully" << endl;
    } catch (Fam_Exception &e) {
        cout << "FAM finalization failed: " << e.fam_error_msg() << endl;
        return 1;
    }

    free(gBuf);
    delete my_fam;

    return 0;
}
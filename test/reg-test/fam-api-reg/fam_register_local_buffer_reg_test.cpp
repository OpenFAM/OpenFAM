#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <pthread.h>

#include <fam/fam.h>
#include <fam/fam_exception.h>

#include "common/fam_test_config.h"

using namespace std;
using namespace openfam;

fam *my_fam;
Fam_Options fam_opts;
Fam_Region_Descriptor *testRegionDesc;
void *default_buf;

#define REGION_SIZE (1024 * 1024)
#define REGION_PERM 0777
#define DEFAULT_BUFFER_SIZE (2 * 1024 * 1024) // 2 MiB
#define BLOCK_SIZE (64 * 1024)                // 64 KiB

TEST(FamRegisterLocalBuffer, DefaultBufferIO) {
    Fam_Descriptor *item = nullptr;
    EXPECT_NO_THROW(item = my_fam->fam_allocate("test_item", BLOCK_SIZE, 0777,
                                                testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    memset(default_buf, 'A', BLOCK_SIZE);

    EXPECT_NO_THROW(my_fam->fam_put_blocking(default_buf, item, 0, BLOCK_SIZE));
    EXPECT_NO_THROW(my_fam->fam_get_blocking(default_buf, item, 0, BLOCK_SIZE));

    for (size_t i = 0; i < BLOCK_SIZE; i++) {
        EXPECT_EQ(((char *)default_buf)[i], 'A');
    }

    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
}

TEST(FamRegisterLocalBuffer, LocalBufferIO) {
    Fam_Descriptor *item = nullptr;
    EXPECT_NO_THROW(item = my_fam->fam_allocate("test_item", BLOCK_SIZE, 0777,
                                                testRegionDesc));
    EXPECT_NE((void *)NULL, item);

    char *writeData = (char *)malloc(BLOCK_SIZE);
    char *readData = (char *)malloc(BLOCK_SIZE);
    memset(writeData, 'B', BLOCK_SIZE);

    EXPECT_NO_THROW(my_fam->fam_register_local_buffer(writeData, BLOCK_SIZE));
    EXPECT_NO_THROW(my_fam->fam_register_local_buffer(readData, BLOCK_SIZE));

    EXPECT_NO_THROW(my_fam->fam_put_blocking(writeData, item, 0, BLOCK_SIZE));
    EXPECT_NO_THROW(my_fam->fam_get_blocking(readData, item, 0, BLOCK_SIZE));

    EXPECT_EQ(memcmp(writeData, readData, BLOCK_SIZE), 0);

    EXPECT_NO_THROW(my_fam->fam_deregister_local_buffer(writeData, BLOCK_SIZE));
    EXPECT_NO_THROW(my_fam->fam_deregister_local_buffer(readData, BLOCK_SIZE));

    free(writeData);
    free(readData);
    EXPECT_NO_THROW(my_fam->fam_deallocate(item));
    delete item;
}

TEST(FamRegisterLocalBuffer, PartiallyOverlappedBufferRegistration) {
    // Partial overlap with the default buffer
    void *new_local_buf = (char *)default_buf + (DEFAULT_BUFFER_SIZE / 2);
    size_t new_local_buf_size = DEFAULT_BUFFER_SIZE;
    EXPECT_THROW(
        my_fam->fam_register_local_buffer(new_local_buf, new_local_buf_size),
        Fam_Exception);

    new_local_buf = (char *)default_buf - (DEFAULT_BUFFER_SIZE / 2);
    new_local_buf_size = DEFAULT_BUFFER_SIZE;
    EXPECT_THROW(
        my_fam->fam_register_local_buffer(new_local_buf, new_local_buf_size),
        Fam_Exception);

    // Partial overlap with the new local buffer
    new_local_buf = malloc(DEFAULT_BUFFER_SIZE);
    new_local_buf_size = DEFAULT_BUFFER_SIZE;
    EXPECT_NO_THROW(
        my_fam->fam_register_local_buffer(new_local_buf, new_local_buf_size));

    void *overlap_buf = (char *)new_local_buf + (DEFAULT_BUFFER_SIZE / 2);
    size_t overlap_buf_size = DEFAULT_BUFFER_SIZE;
    EXPECT_THROW(
        my_fam->fam_register_local_buffer(overlap_buf, overlap_buf_size),
        Fam_Exception);

    overlap_buf = (char *)new_local_buf - (DEFAULT_BUFFER_SIZE / 2);
    overlap_buf_size = DEFAULT_BUFFER_SIZE;
    EXPECT_THROW(
        my_fam->fam_register_local_buffer(overlap_buf, overlap_buf_size),
        Fam_Exception);

    EXPECT_NO_THROW(
        my_fam->fam_deregister_local_buffer(new_local_buf, new_local_buf_size));
    free(new_local_buf);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    my_fam = new fam();
    init_fam_options(&fam_opts);

    default_buf = (char *)malloc(DEFAULT_BUFFER_SIZE);

    fam_opts.local_buf_addr = (char *)default_buf;
    fam_opts.local_buf_size = DEFAULT_BUFFER_SIZE;
    fam_opts.famThreadModel = strdup("FAM_THREAD_MULTIPLE");

    EXPECT_NO_THROW(my_fam->fam_initialize("default", &fam_opts));

    EXPECT_NO_THROW(testRegionDesc = my_fam->fam_create_region(
                        "test_region", REGION_SIZE, REGION_PERM, NULL));
    EXPECT_NE((void *)NULL, testRegionDesc);

    int ret = RUN_ALL_TESTS();

    EXPECT_NO_THROW(my_fam->fam_destroy_region(testRegionDesc));
    delete testRegionDesc;
    free(default_buf);

    EXPECT_NO_THROW(my_fam->fam_finalize("default"));

    delete my_fam;

    return ret;
}
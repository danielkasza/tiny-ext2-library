/* Copyright (c) 2020, Daniel Kasza
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ext2.h"

#include <stdio.h>
#include <inttypes.h>

/* Implement the disk access callback of the Ext2 library. */
static const char *disk_access(void *context, uint32_t first, uint32_t count, uint8_t *buffer) {
    FILE *f = (FILE*)context;

    fprintf(stderr, "DISK ACCESS: first=%" PRIu32 " count=%" PRIu32 "\n", first, count);

    /* Seek to the correct offset. */
    if (fseek(f, (first*512), SEEK_SET) != 0) {
        return "seek failed";
    }

    /* Read the data. */
    if (fread(buffer, 512, count, f) != count) {
        // FIXME: this could fail if fread returned fewer sectors than requested.
        return "read failed";
    }

    return NULL;
}

static void handle_error(const char *function, const char *error) {
    if (error != NULL) {
        fprintf(stderr, "%s: %s\n", function, error);
        exit(EXIT_FAILURE);
    }
}

/* Chunk of memory for cache. */
static char cache_memory[EXT2_FS_CACHE_BLOCKS_COUNT_MAX * 4096];

int main(int argc, char *argv[]) {
    const char *error = NULL;

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments!\n");
        return EXIT_FAILURE;
    }

    /* Open the Ext2 filesystem file. */
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        handle_error("fopen", "could not open file");
    }

    /* Initialize the Ext2 library. */
    ext2_fs_t fs = { 0 };
    error = ext2_open_fs(
        &disk_access, f,
        cache_memory, sizeof(cache_memory),
        &fs
    );
    handle_error("ext2_open_fs", error);

    /* You should not do this!
     * These are private fields, but we want to verify that they are correct.
     */
    printf("fs.blocks_count         = %" PRIu32 "\n", fs.blocks_count);
    printf("fs.block_size           = %" PRIu32 "\n", fs.block_size);
    printf("fs.blocks_per_group     = %" PRIu32 "\n", fs.blocks_per_group);
    printf("fs.inodes_per_group     = %" PRIu32 "\n", fs.inodes_per_group);
    printf("fs.cache_blocks_count   = %zu\n",         fs.cache_blocks_count);

    /* Get the inode for "/boot/Image".*/
    const char *path[] = { "boot", "Image", NULL };
    ext2_inode_t image_inode = { 0 };
    error = ext2_get_inode_by_path(&fs, path, &image_inode);
    handle_error("ext2_get_inode_by_idx", error);
    printf("image_inode.uid         = %" PRIu16 "\n", image_inode.uid);
    printf("image_inode.size        = %" PRIu32 "\n", image_inode.size);
    printf("image_inode.gid         = %" PRIu16 "\n", image_inode.gid);
    printf("image_inode.links_count = %" PRIu16 "\n", image_inode.links_count);
    printf("image_inode.blocks      = %" PRIu32 "\n", image_inode.blocks);

    /* Read the Image. */
    uint8_t *image_data = malloc(image_inode.size);
    if (!image_data) {
        handle_error("malloc", "memory allocation failed");
    }
    error = ext2_read(&fs, &image_inode, 0, image_inode.size, image_data);
    handle_error("ext2_read", error);

    /* Write it to a file. */
    FILE *out = fopen("Image", "w");
    if (out == NULL) {
        handle_error("fopen", "could not open file");
    }
    // FIXME: should check the return value
    (void)fwrite(image_data, image_inode.size, 1, out);

    /* Print cache information. */
    printf("fs.cache_hits           = %" PRIu32 "\n", fs.cache_hits);
    printf("fs.cache_misses         = %" PRIu32 "\n", fs.cache_misses);

    return EXIT_SUCCESS;
}
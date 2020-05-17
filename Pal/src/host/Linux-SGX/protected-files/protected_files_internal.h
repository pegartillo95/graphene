/* Copyright (C) 2019-2020 Invisible Things Lab
                           Rafal Wojdyla <omeg@invisiblethingslab.com>

   This file is part of Graphene Library OS.

   Graphene Library OS is free software: you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   Graphene Library OS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/*
 * Copyright (C) 2011-2019 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef PROTECTED_FILES_INTERNAL_H_
#define PROTECTED_FILES_INTERNAL_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "list.h"
#include "lru_cache.h"
#include "protected_files.h"
#include "protected_file_format.h"

#define MAX_PAGES_IN_CACHE 48

typedef enum {
    FILE_MHT_NODE_TYPE = 1,
    FILE_DATA_NODE_TYPE = 2,
} mht_node_type_e;

DEFINE_LIST(_file_node);
typedef struct _file_node {
    LIST_TYPE(_file_node) list;
    uint8_t type;
    uint64_t node_number;
    struct _file_node* parent;
    bool need_writing;
    bool new_node;
    union {
        struct {
            uint64_t physical_node_number;
            encrypted_node_t encrypted; // the actual data from the disk
        };
        recovery_node_t recovery_node;
    };
    union { // decrypted data
        mht_node_t mht_plain;
        data_node_t data_plain;
    };
} file_node_t;
DEFINE_LISTP(_file_node);

struct pf_context {
    union {
        struct {
            uint64_t meta_data_node_number; // for recovery purpose, so it is easy to write this node
            meta_data_node_t file_meta_data; // actual data from disk's meta data node
        };
        recovery_node_t meta_data_recovery_node;
    };

    meta_data_encrypted_t encrypted_part_plain; // encrypted part of meta data node, decrypted
    file_node_t root_mht; // the root of the mht is always needed (for files bigger than 3KB)
    pf_handle_t file;
    pf_file_mode_t mode;
    int64_t offset; // current file position (user's view)
    bool end_of_file;
    size_t real_file_size;
    bool need_writing;
    pf_status_t file_status;
    pf_key_t user_kdk_key;
    pf_key_t cur_key;
    pf_key_t session_master_key;
    uint32_t master_key_count;
    char recovery_filename[RECOVERY_FILE_MAX_LEN]; // might include full path to the file
    lruc_context_t cache;
#ifdef DEBUG
    char* debug_buffer; // buffer for debug output
#endif
};

static bool ipf_init_fields(pf_context_t pf);
static bool ipf_file_recovery(pf_context_t pf, const char* filename);
static bool ipf_init_existing_file(pf_context_t pf, const char* filename);
static bool ipf_init_new_file(pf_context_t pf, const char* clean_filename);

static bool ipf_read_node(pf_context_t pf, pf_handle_t handle, uint64_t node_number, void* buffer,
                          uint32_t node_size);
static bool ipf_write_node(pf_context_t pf, pf_handle_t handle, uint64_t node_number, void* buffer,
                           uint32_t node_size);

static bool ipf_generate_secure_blob(pf_context_t pf, pf_key_t* key, const char* label,
                                     uint64_t physical_node_number, pf_mac_t* output);
static bool ipf_generate_secure_blob_from_user_kdk(pf_context_t pf, bool restore);
static bool ipf_init_session_master_key(pf_context_t pf);
static bool ipf_derive_random_node_key(pf_context_t pf, uint64_t physical_node_number);
static bool ipf_generate_random_meta_data_key(pf_context_t pf);
static bool ipf_restore_current_meta_data_key(pf_context_t pf);

static file_node_t* ipf_get_data_node(pf_context_t pf);
static file_node_t* ipf_read_data_node(pf_context_t pf);
static file_node_t* ipf_append_data_node(pf_context_t pf);
static file_node_t* ipf_get_mht_node(pf_context_t pf);
static file_node_t* ipf_read_mht_node(pf_context_t pf, uint64_t mht_node_number);
static file_node_t* ipf_append_mht_node(pf_context_t pf, uint64_t mht_node_number);

static bool ipf_write_recovery_file(pf_context_t pf);
static bool ipf_set_update_flag(pf_context_t pf, bool flush_to_disk);
static void ipf_clear_update_flag(pf_context_t pf);
static bool ipf_update_all_data_and_mht_nodes(pf_context_t pf);
static bool ipf_update_meta_data_node(pf_context_t pf);
static bool ipf_write_all_changes_to_disk(pf_context_t pf, bool flush_to_disk);
static bool ipf_erase_recovery_file(pf_context_t pf);
static bool ipf_internal_flush(pf_context_t pf, bool flush_to_disk);
static bool ipf_do_file_recovery(pf_context_t pf, const char* filename, uint32_t node_size);
static bool ipf_pre_close(pf_context_t pf);

static pf_context_t ipf_open(const char* filename, pf_file_mode_t mode, bool create,
                             pf_handle_t file, size_t real_size, const pf_key_t* kdk_key,
                             bool enable_recovery);
static bool ipf_close(pf_context_t pf);
static size_t ipf_read(pf_context_t pf, void* ptr, size_t size);
static size_t ipf_write(pf_context_t pf, const void* ptr, size_t size);
static bool ipf_seek(pf_context_t pf, int64_t new_offset);
static void ipf_clear_error(pf_context_t pf);

#endif

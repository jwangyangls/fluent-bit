/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  CMetrics
 *  ========
 *  Copyright 2021 Eduardo Silva <eduardo@calyptia.com>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


#ifndef CMT_MPACK_UTILS_H
#define CMT_MPACK_UTILS_H

#include <cmetrics/cmetrics.h>
#include <cmetrics/cmt_sds.h>

#include <mpack/mpack.h>

#define CMT_MPACK_SUCCESS                    0
#define CMT_MPACK_INVALID_ARGUMENT_ERROR     1
#define CMT_MPACK_ALLOCATION_ERROR           2
#define CMT_MPACK_CORRUPT_INPUT_DATA_ERROR   3
#define CMT_MPACK_CONSUME_ERROR              4
#define CMT_MPACK_ENGINE_ERROR               5
#define CMT_MPACK_PENDING_MAP_ENTRIES        6
#define CMT_MPACK_PENDING_ARRAY_ENTRIES      7
#define CMT_MPACK_UNEXPECTED_KEY_ERROR       8
#define CMT_MPACK_UNEXPECTED_DATA_TYPE_ERROR 9
#define CMT_MPACK_ERROR_CUTOFF               20

#define CMT_MPACK_MAX_ARRAY_ENTRY_COUNT      65535
#define CMT_MPACK_MAX_MAP_ENTRY_COUNT        10
#define CMT_MPACK_MAX_STRING_LENGTH          1024

typedef int (*cmt_mpack_unpacker_entry_callback_fn_t)(mpack_reader_t *reader, 
                                                      size_t index, void *context);

struct cmt_mpack_map_entry_callback_t {
    const char                            *identifier;
    cmt_mpack_unpacker_entry_callback_fn_t handler;
};

int cmt_mpack_consume_double_tag(mpack_reader_t *reader, double *output_buffer);
int cmt_mpack_consume_uint_tag(mpack_reader_t *reader, uint64_t *output_buffer);
int cmt_mpack_consume_string_tag(mpack_reader_t *reader, cmt_sds_t *output_buffer);
int cmt_mpack_unpack_map(mpack_reader_t *reader, 
                         struct cmt_mpack_map_entry_callback_t *callback_list, 
                         void *context);
int cmt_mpack_unpack_array(mpack_reader_t *reader, 
                           cmt_mpack_unpacker_entry_callback_fn_t entry_processor_callback, 
                           void *context);


#endif

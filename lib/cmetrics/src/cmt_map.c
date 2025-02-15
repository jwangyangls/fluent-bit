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

#include <cmetrics/cmetrics.h>
#include <cmetrics/cmt_map.h>
#include <cmetrics/cmt_sds.h>
#include <cmetrics/cmt_log.h>
#include <cmetrics/cmt_hash.h>
#include <cmetrics/cmt_metric.h>
#include <cmetrics/cmt_compat.h>

struct cmt_map *cmt_map_create(int type, struct cmt_opts *opts, int count, char **labels)
{
    int i;
    char *name;
    struct cmt_map *map;
    struct cmt_map_label *label;

    if (count < 0) {
        return NULL;
    }

    map = calloc(1, sizeof(struct cmt_map));
    if (!map) {
        cmt_errno();
        return NULL;
    }
    map->type = type;
    map->opts = opts;
    map->label_count = count;
    mk_list_init(&map->label_keys);
    mk_list_init(&map->metrics);
    mk_list_init(&map->metric.labels);

    if (count == 0) {
        map->metric_static_set = 1;
    }

    for (i = 0; i < count; i++) {
        label = malloc(sizeof(struct cmt_map_label));
        if (!label) {
            cmt_errno();
            goto error;
        }

        name = labels[i];
        label->name = cmt_sds_create(name);
        if (!label->name) {
            cmt_errno();
            free(label);
            goto error;
        }
        mk_list_add(&label->_head, &map->label_keys);
    }

    return map;

 error:
    cmt_map_destroy(map);
    return NULL;
}

static struct cmt_metric *metric_hash_lookup(struct cmt_map *map, uint64_t hash)
{
    struct mk_list *head;
    struct cmt_metric *metric;

    if (hash == 0) {
        return &map->metric;
    }

    mk_list_foreach(head, &map->metrics) {
        metric = mk_list_entry(head, struct cmt_metric, _head);
        if (metric->hash == hash) {
            return metric;
        }
    }

    return NULL;
}

static struct cmt_metric *map_metric_create(uint64_t hash,
                                            int labels_count, char **labels_val)
{
    int i;
    char *name;
    struct cmt_metric *metric;
    struct cmt_map_label *label;

    metric = malloc(sizeof(struct cmt_metric));
    if (!metric) {
        cmt_errno();
        return NULL;
    }
    mk_list_init(&metric->labels);
    metric->val = 0.0;
    metric->hash = hash;

    for (i = 0; i < labels_count; i++) {
        label = malloc(sizeof(struct cmt_map_label));
        if (!label) {
            cmt_errno();
            goto error;
        }

        name = labels_val[i];
        label->name = cmt_sds_create(name);
        if (!label->name) {
            cmt_errno();
            free(label);
            goto error;
        }
        mk_list_add(&label->_head, &metric->labels);
    }

    return metric;

 error:
    free(metric);
    return NULL;
}

static void map_metric_destroy(struct cmt_metric *metric)
{
    struct mk_list *tmp;
    struct mk_list *head;
    struct cmt_map_label *label;

    mk_list_foreach_safe(head, tmp, &metric->labels) {
        label = mk_list_entry(head, struct cmt_map_label, _head);
        cmt_sds_destroy(label->name);
        mk_list_del(&label->_head);
        free(label);
    }

    mk_list_del(&metric->_head);
    free(metric);
}

struct cmt_metric *cmt_map_metric_get(struct cmt_opts *opts, struct cmt_map *map,
                                      int labels_count, char **labels_val,
                                      int write_op)
{
    int i;
    int len;
    int found = 0;
    char *ptr;
    uint64_t hash;
    XXH64_state_t state;
    struct cmt_metric *metric = NULL;

    /* Enforce zero or exact labels */
    if (labels_count > 0 && labels_count != map->label_count) {
        return NULL;
    }

    /*
     * If the caller wants the no-labeled metric (metric_static_set) make sure
     * it was already pre-defined.
     */
    if (labels_count == 0 && labels_val == NULL) {
        /*
         * if an upcoming 'write operation' will be performed for a default
         * static metric, just initialize it and return it.
         */
        if (map->metric_static_set) {
            metric = &map->metric;
        }
        else if (write_op) {
            metric = &map->metric;
            if (!map->metric_static_set) {
                map->metric_static_set = 1;
            }
        }

        /* return the proper context or NULL */
        return metric;
    }

    /* Lookup the metric */
    XXH64_reset(&state, 0);
    XXH64_update(&state, opts->fqname, cmt_sds_len(opts->fqname));
    for (i = 0; i < labels_count; i++) {
        ptr = labels_val[i];
        if (!ptr) {
            return NULL;
        }
        len = strlen(ptr);
        XXH64_update(&state, ptr, len);
    }

    hash = XXH64_digest(&state);
    metric = metric_hash_lookup(map, hash);

    if (metric) {
        return metric;
    }

    /*
     * If the metric was not found and the caller will not write a value, just
     * return NULL.
     */
    if (!write_op) {
        return NULL;
    }

    /* If the metric has not been found, just create it */
    metric = map_metric_create(hash, labels_count, labels_val);
    if (!metric) {
        return NULL;
    }
    mk_list_add(&metric->_head, &map->metrics);
    return metric;
}


int cmt_map_metric_get_val(struct cmt_opts *opts, struct cmt_map *map,
                           int labels_count, char **labels_val,
                           double *out_val)
{
    double val = 0;
    struct cmt_metric *metric;

    metric = cmt_map_metric_get(opts, map, labels_count, labels_val, CMT_FALSE);
    if (!metric) {
        return -1;
    }

    val = cmt_metric_get_value(metric);
    *out_val = val;
    return 0;
}

void cmt_map_destroy(struct cmt_map *map)
{
    struct mk_list *tmp;
    struct mk_list *head;
    struct cmt_map_label *label;
    struct cmt_metric *metric;

    mk_list_foreach_safe(head, tmp, &map->label_keys) {
        label = mk_list_entry(head, struct cmt_map_label, _head);
        cmt_sds_destroy(label->name);
        mk_list_del(&label->_head);
        free(label);
    }

    mk_list_foreach_safe(head, tmp, &map->metrics) {
        metric = mk_list_entry(head, struct cmt_metric, _head);
        map_metric_destroy(metric);
    }

    free(map);
}

/* I don't know if we should leave this or promote the label type so it has its own
 * header and source files with their own constructor / destructor and an agnostic name.
 * That last bit comes from the fact that we are using the cmt_map_label type both in the
 * dimension definition list held by the map structure and the dimension value list held
 * by the metric structure.
 */

void destroy_label_list(struct mk_list *label_list)
{
    struct mk_list       *tmp;
    struct mk_list       *head;
    struct cmt_map_label *label;

    mk_list_foreach_safe(head, tmp, label_list) {
        label = mk_list_entry(head, struct cmt_map_label, _head);

        cmt_sds_destroy(label->name);

        mk_list_del(&label->_head);

        free(label);
    }
}

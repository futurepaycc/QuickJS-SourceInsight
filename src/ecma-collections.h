//
// Created by benpeng.jiang on 2021/6/19.
//

#ifndef LOX_JS_COLLECTIONS_H
#define LOX_JS_COLLECTIONS_H
#include "qjs-api.h"

/* Set/Map/WeakSet/WeakMap */

typedef struct JSMapRecord {
    int ref_count; /* used during enumeration to avoid freeing the record */
    BOOL empty; /* TRUE if the record is deleted */
    struct JSMapState *map;
    struct JSMapRecord *next_weak_ref;
    struct list_head link;
    struct list_head hash_link;
    JSValue key;
    JSValue value;
} JSMapRecord;

typedef struct JSMapState {
    BOOL is_weak; /* TRUE if WeakSet/WeakMap */
    struct list_head records; /* list of JSMapRecord.link */
    uint32_t record_count;
    struct list_head *hash_table;
    uint32_t hash_size; /* must be a power of two */
    uint32_t record_count_threshold; /* count at which a hash table
                                        resize is needed */
} JSMapState;

#endif //LOX_JS_COLLECTIONS_H

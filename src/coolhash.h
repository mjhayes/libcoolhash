#ifndef __LIBCOOLHASH_COOLHASH_H__
#define __LIBCOOLHASH_COOLHASH_H__

#include <pthread.h>
#include <stdint.h>

struct coolhash;

typedef uint64_t coolhash_key_t;
typedef void (*coolhash_free_foreach_func)(void *data, void *cb_arg);
typedef void (*coolhash_foreach_func)(struct coolhash *ch, coolhash_key_t key,
                void *data, void *lock, void *cb_arg);

struct coolhash_profile {
        unsigned int size; /**< Initial and minimum hash table size */
        unsigned int shards; /**< Number of shards */
        int load_factor; /**< Load factor before resize, in percent */
};

struct coolhash_node {
        coolhash_key_t key; /**< Node key */
        struct coolhash_node *next; /**< Next node */
        pthread_rwlock_t node_mx;

        int del; /**< Set to 1 when scheduled for deletion */
        void *data; /**< Node data */
};

struct coolhash_table {
        unsigned int n; /**< Number of items currently in table */
        unsigned int size; /**< Size of table */
        unsigned int grow_at; /**< When to grow */
        unsigned int shrink_at; /**< When to shrink */
        pthread_mutex_t table_mx;

        struct coolhash_node **nodes; /**< Table nodes */
};

struct coolhash {
        struct coolhash_profile profile; /**< Configuration profile */

        struct coolhash_table *tables;
};

struct coolhash *coolhash_new(struct coolhash_profile *profile);
void coolhash_free(struct coolhash *ch);
void coolhash_free_foreach(struct coolhash *ch, coolhash_free_foreach_func cb,
        void *cb_arg);
void coolhash_profile_init(struct coolhash_profile *profile);
void coolhash_profile_set_size(struct coolhash_profile *profile,
                unsigned int size);
unsigned int coolhash_profile_get_size(struct coolhash_profile *profile);
void coolhash_profile_set_shards(struct coolhash_profile *profile,
                unsigned int shards);
unsigned int coolhash_profile_get_shards(struct coolhash_profile *profile);
void coolhash_profile_set_load_factor(struct coolhash_profile *profile,
                int load_factor);
int coolhash_profile_get_load_factor(struct coolhash_profile *profile);
int coolhash_set(struct coolhash *ch, coolhash_key_t key, void *data);
void *coolhash_get(struct coolhash *ch, coolhash_key_t key, void **lock);
void *coolhash_get_ro(struct coolhash *ch, coolhash_key_t key,
                void **lock);
int coolhash_get_copy(struct coolhash *ch, coolhash_key_t key, void *dst,
                size_t dst_len);
void coolhash_del(struct coolhash *ch, void *lock);
void coolhash_unlock(struct coolhash *ch, void *lock);
void coolhash_foreach(struct coolhash *ch, coolhash_foreach_func cb,
                void *cb_arg);
void coolhash_foreach_ro(struct coolhash *ch, coolhash_foreach_func cb,
                void *cb_arg);

#endif /* __LIBCOOLHASH_COOLHASH_H__ */

/* vim: set et ts=8 sw=8 sts=8: */

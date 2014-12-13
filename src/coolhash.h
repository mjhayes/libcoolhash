#ifndef __LIBCOOLHASH_COOLHASH_H__
#define __LIBCOOLHASH_COOLHASH_H__

#include <pthread.h>
#include <stdint.h>

typedef uint64_t coolhash_key_t;
typedef void (*coolhash_free_foreach_func)(void *data, void *cb_arg);
typedef void (*coolhash_foreach_func)(void *data, void *lock, void *cb_arg);

struct coolhash_profile {
        long int size; /**< Initial hash table size */
        long int shards; /**< Number of shards */
};

struct coolhash_node {
        coolhash_key_t key; /**< Node key */
        struct coolhash_node *next; /**< Next node */
        pthread_mutex_t node_mx;

        int del; /**< Set to 1 when scheduled for deletion */
        void *data; /**< Node data */
        int refs; /**< Number of references to this node */
};

struct coolhash_table {
        long int n; /**< Number of items currently in table */
        long int size; /**< Size of table */
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
int coolhash_set(struct coolhash *ch, coolhash_key_t key, void *data);
void *coolhash_get(struct coolhash *ch, coolhash_key_t key, void **lock);
int coolhash_get_copy(struct coolhash *ch, coolhash_key_t key, void *dst,
                size_t dst_len);
void coolhash_del(struct coolhash *ch, void *lock);
void coolhash_unlock(void *lock);

#endif /* __LIBCOOLHASH_COOLHASH_H__ */

/* vim: set et ts=8 sw=8 sts=8: */

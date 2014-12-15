#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "inc.h"

#define COOLHASH_DEFAULT_PROFILE_SIZE 10 /**< Initial hash table size (should
                                           be exactly divisible by SHARDS */
#define COOLHASH_DEFAULT_PROFILE_SHARDS 2 /**< Number of shards */
#define COOLHASH_DEFAULT_PROFILE_LOAD_FACTOR 80 /**< Load factor (percent) */

static void _coolhash_table_add(struct coolhash_table *table,
                struct coolhash_node *node);
static struct coolhash_table *_coolhash_table_find(struct coolhash *ch,
                coolhash_key_t key);
static void _coolhash_table_lock(struct coolhash_table *table);
static void _coolhash_table_unlock(struct coolhash_table *table);
static void _coolhash_table_auto_rehash(struct coolhash *ch,
                struct coolhash_table *table);
static void _coolhash_table_grow_shrink_calc(struct coolhash *ch,
                struct coolhash_table *table);
static struct coolhash_node *_coolhash_node_find(struct coolhash *ch,
                coolhash_key_t key, struct coolhash_table **table_ptr,
                int table_unlock);
static void _coolhash_node_lock(struct coolhash_node *node);
static void _coolhash_node_unlock(struct coolhash_node *node);

/**
 * @brief Initialize new coolhash instance
 *
 * @param profile Configuration profile (pass in NULL for defaults)
 *
 * @return New coolhash instance or NULL on failure
 */
struct coolhash *coolhash_new(struct coolhash_profile *profile)
{
        struct coolhash *ch;
        unsigned int i, j;

        ch = malloc(sizeof(*ch));
        if (ch == NULL)
                return NULL;

        if (profile)
                memcpy(&ch->profile, profile, sizeof(ch->profile));
        else
                coolhash_profile_init(&ch->profile);

        /* Make configuration values sane if they aren't already */
        if (ch->profile.size <= 0)
                ch->profile.size = 1;
        if (ch->profile.shards < 1)
                ch->profile.shards = 1;
        if (ch->profile.size < ch->profile.shards)
                ch->profile.size = ch->profile.shards;
        if (ch->profile.size % ch->profile.shards != 0)
                ch->profile.size += ch->profile.size % ch->profile.shards;
        if (ch->profile.load_factor <= 0)
                ch->profile.load_factor = COOLHASH_DEFAULT_PROFILE_LOAD_FACTOR;

        /* Initialize hash tables */
        ch->tables = calloc(ch->profile.shards, sizeof(*ch->tables));
        if (ch->tables == NULL) {
                free(ch);
                return NULL;
        }

        for (i = 0; i < ch->profile.shards; i++) {
                ch->tables[i].n = 0;
                ch->tables[i].size = ch->profile.size / ch->profile.shards;
                _coolhash_table_grow_shrink_calc(ch, &ch->tables[i]);

                ch->tables[i].nodes = calloc(ch->tables[i].size,
                                sizeof(*ch->tables[i].nodes));
                if (ch->tables[i].nodes == NULL)
                        break;
                if (pthread_mutex_init(&ch->tables[i].table_mx, NULL) != 0) {
                        free(ch->tables[i].nodes);
                        break;
                }
        }

        /* There was a failure, free up memory */
        if (i < ch->profile.shards) {
                for (j = 0; j < i - 1; j++) {
                        free(ch->tables[j].nodes);
                        pthread_mutex_destroy(&ch->tables[j].table_mx);
                }

                free(ch->tables);
                free(ch);
                return NULL;
        }

        return ch;
}

/**
 * @brief
 *
 * @param ch
 */
void coolhash_free(struct coolhash *ch)
{
        coolhash_free_foreach(ch, NULL, NULL);
}

/**
 * @brief
 *
 * @param profile
 */
void coolhash_profile_init(struct coolhash_profile *profile)
{
        profile->size = COOLHASH_DEFAULT_PROFILE_SIZE;
        profile->shards = COOLHASH_DEFAULT_PROFILE_SHARDS;
        profile->load_factor = COOLHASH_DEFAULT_PROFILE_LOAD_FACTOR;
}

/**
 * @brief
 *
 * @param profile
 * @param size
 */
void coolhash_profile_set_size(struct coolhash_profile *profile,
                unsigned int size)
{
        profile->size = size;
}

/**
 * @brief
 *
 * @param profile
 *
 * @return
 */
unsigned int coolhash_profile_get_size(struct coolhash_profile *profile)
{
        return profile->size;
}

/**
 * @brief
 *
 * @param profile
 * @param shards
 */
void coolhash_profile_set_shards(struct coolhash_profile *profile,
                unsigned int shards)
{
        profile->shards = shards;
}

/**
 * @brief
 *
 * @param profile
 *
 * @return
 */
unsigned int coolhash_profile_get_shards(struct coolhash_profile *profile)
{
        return profile->shards;
}

/**
 * @brief
 *
 * @param profile
 * @param load_factor
 */
void coolhash_profile_set_load_factor(struct coolhash_profile *profile,
                int load_factor)
{
        profile->load_factor = load_factor;
}

/**
 * @brief
 *
 * @param profile
 *
 * @return
 */
int coolhash_profile_get_load_factor(struct coolhash_profile *profile)
{
        return profile->load_factor;
}

/**
 * @brief
 *
 * @param ch
 * @param cb
 * @param cb_arg
 */
void coolhash_free_foreach(struct coolhash *ch, coolhash_free_foreach_func cb,
        void *cb_arg)
{
        unsigned int i, j;
        struct coolhash_node *n, *nn;

        if (ch == NULL)
                return;

        for (i = 0; i < ch->profile.shards; i++) {
                for (j = 0; j < ch->tables[i].size; j++) {
                        if (ch->tables[i].nodes[j] == NULL)
                                continue;

                        for (n = ch->tables[i].nodes[j]; n; n = nn) {

                                if (cb)
                                        cb(n->data, cb_arg);

                                nn = n->next;
                                free(n);
                        }
                }

                free(ch->tables[i].nodes);
        }

        free(ch->tables);
        free(ch);
}

/**
 * @brief
 *
 * @param ch
 * @param key
 * @param data
 *
 * @return
 */
int coolhash_set(struct coolhash *ch, coolhash_key_t key, void *data)
{
        struct coolhash_node *node;
        struct coolhash_table *table;

        if (ch == NULL || data == NULL)
                return -1;

        node = _coolhash_node_find(ch, key, &table, 0);
        if (node) {
                /* A node already exists. We just need to overwrite the data
                 * and make sure to unschedule deletion if that's the case. */

                node->del = 0; /* Could have been scheduled for deletion */
                node->data = data;
                _coolhash_node_unlock(node);

                goto leave;
        }

        /* This is a totally new node */
        node = malloc(sizeof(*node));
        if (node == NULL)
                return -1;

        node->key = key;
        if (pthread_mutex_init(&node->node_mx, NULL) != 0) {
                free(node);
                return -1;
        }
        node->del = 0;
        node->data = data;

        /* Add new node */
        _coolhash_table_add(table, node);
        table->n++;
        _coolhash_table_auto_rehash(ch, table);

leave:
        _coolhash_table_unlock(table);
        return 0;
}

/**
 * @brief
 *
 * @param ch
 * @param key
 * @param lock
 *
 * @return
 */
void *coolhash_get(struct coolhash *ch, coolhash_key_t key, void **lock)
{
        struct coolhash_node *node;

        if (ch == NULL || lock == NULL)
                return NULL;

        node =_coolhash_node_find(ch, key, NULL, 1);
        if (node == NULL)
                return NULL;

        if (node->del) {
                _coolhash_node_unlock(node);
                return NULL;
        }

        *lock = node;
        return node->data;
}

/**
 * @brief Get data and copy into destination buffer
 *
 * @param ch
 * @param key
 * @param dst
 * @param dst_len
 *
 * @return
 */
int coolhash_get_copy(struct coolhash *ch, coolhash_key_t key, void *dst,
                size_t dst_len)
{
        struct coolhash_node *node;

        if (ch == NULL || dst == NULL || dst_len <= 0)
                return -1;

        node = _coolhash_node_find(ch, key, NULL, 1);
        if (node == NULL)
                return -1;

        if (node->del) {
                _coolhash_node_unlock(node);
                return -1;
        }

        memcpy(dst, node->data, dst_len);
        _coolhash_node_unlock(node);

        return 0;
}

/**
 * @brief So, to delete a node you need to 'get' it first. That's so you can
 * do whatever freeing is necessary and you'll then pass the lock pointer
 * to this function.
 *
 * @param ch coolhash instance
 * @param lock Pointer you got from the 'get' function.
 */
void coolhash_del(struct coolhash *ch, void *lock)
{
        struct coolhash_table *table;
        struct coolhash_node *node;

        if (lock == NULL)
                return;

        node = lock;
        node->del = 1;

        table = _coolhash_table_find(ch, node->key);

        _coolhash_table_lock(table);
        table->n--;
        _coolhash_table_auto_rehash(ch, table);
        _coolhash_table_unlock(table);

        _coolhash_node_unlock(node);
}

/**
 * @brief Unlock after a 'get'
 *
 * @param ch coolhash instance
 * @param lock Pointer you got from the 'get' function.
 */
void coolhash_unlock(struct coolhash *ch, void *lock)
{
        struct coolhash_node *node;

        if (lock == NULL)
                return;

        node = lock;
        _coolhash_node_unlock(node);
}

/**
 * @brief Loop through every node in the hash table
 *
 * @param ch coolhash instance
 * @param cb Callback function (required) - The callback function must pass
 * the 'lock' parameter to coolhash_unlock or coolhash_del before returning!
 * @param cb_arg Callback function argument (optional)
 */
void coolhash_foreach(struct coolhash *ch, coolhash_foreach_func cb,
                void *cb_arg)
{
        unsigned int i, j;
        struct coolhash_node *n;

        if (ch == NULL || cb == NULL)
                return;

        for (i = 0; i < ch->profile.shards; i++) {
                _coolhash_table_lock(&ch->tables[i]);
                for (j = 0; j < ch->tables[i].size; j++) {
                        for (n = ch->tables[i].nodes[j]; n; n = n->next) {
                                _coolhash_node_lock(n);
                                if (n->del) {
                                        _coolhash_node_unlock(n);
                                        continue;
                                }

                                cb(ch, n->key, n->data, n, cb_arg);
                                /* The callback needs to unlock or delete the
                                 * node. */
                        }
                }
                _coolhash_table_unlock(&ch->tables[i]);
        }
}

/**
 * @brief Lock a node
 *
 * @param node Node
 */
static void _coolhash_node_lock(struct coolhash_node *node)
{
        pthread_mutex_lock(&node->node_mx);
}

/**
 * @brief Unlock a node
 *
 * @param node Node
 */
static void _coolhash_node_unlock(struct coolhash_node *node)
{
        pthread_mutex_unlock(&node->node_mx);
}

/**
 * @brief Find node - make sure to unlock the node_mx when done (if a node is
 * found)
 *
 * @param ch
 * @param key
 * @param table_ptr Fill in a table pointer if you need this info
 * @param table_unlock Boolean, unlock the table when done?
 *
 * @return
 */
static struct coolhash_node *_coolhash_node_find(struct coolhash *ch,
                coolhash_key_t key, struct coolhash_table **table_ptr,
                int table_unlock)
{
        struct coolhash_table *table;
        struct coolhash_node *node;

        table = _coolhash_table_find(ch, key);
        if (table_ptr)
                *table_ptr = table;

        _coolhash_table_lock(table);

        node = table->nodes[key % table->size];
        for (; node && node->key != key; node = node->next)
                ;
        if (node)
                _coolhash_node_lock(node);

        if (table_unlock)
                _coolhash_table_unlock(table);

        return node;
}

/**
 * @brief
 *
 * @param table
 * @param node
 */
static void _coolhash_table_add(struct coolhash_table *table,
                struct coolhash_node *node)
{
        unsigned int idx;

        idx = (unsigned int) (node->key % (coolhash_key_t) table->size);
        node->next = table->nodes[idx];
        table->nodes[idx] = node;
}

/**
 * @brief Find the table 'key' would be in
 *
 * @param ch coolhash instance
 * @param key Key
 *
 * @return The table key would be in
 */
static struct coolhash_table *_coolhash_table_find(struct coolhash *ch,
                coolhash_key_t key)
{
        return &ch->tables[key % ch->profile.shards];
}

/**
 * @brief
 *
 * @param ch
 * @param table
 */
static void _coolhash_table_grow_shrink_calc(struct coolhash *ch,
                struct coolhash_table *table)
{
        table->grow_at = (unsigned int)
                ((uint64_t) table->size * ch->profile.load_factor / 100);

        if (table->size <= ch->profile.size / ch->profile.shards)
                table->shrink_at = 0;
        else
                table->shrink_at = table->grow_at / 5;
}

/**
 * @brief Lock a table
 *
 * @param table Table
 */
static void _coolhash_table_lock(struct coolhash_table *table)
{
        pthread_mutex_lock(&table->table_mx);
}

/**
 * @brief Unlock a table
 *
 * @param table Table
 */
static void _coolhash_table_unlock(struct coolhash_table *table)
{
        pthread_mutex_unlock(&table->table_mx);
}

/**
 * @brief
 *
 * @param ch
 * @param table
 */
static void _coolhash_table_auto_rehash(struct coolhash *ch,
                struct coolhash_table *table)
{
        unsigned int i, nsize, oldsize;
        struct coolhash_node *node, *noden, **oldnodes;

        if (table->n > table->grow_at)
                nsize = table->size * 2;
        else if (table->n < table->shrink_at)
                nsize = table->size / 2;
        else
                return;

        oldnodes = table->nodes;
        table->nodes = calloc(nsize, sizeof(*table->nodes));
        if (table->nodes == NULL) {
                /* Apparently there was not enough memory available to
                 * perform this allocation. Abort! */
                table->nodes = oldnodes;
                return;
        }

        oldsize = table->size;
        table->size = nsize;
        _coolhash_table_grow_shrink_calc(ch, table);

        /* Move nodes to new table and remove nodes marked for deletion */
        for (i = 0; i < oldsize; i++) {
                if (oldnodes[i] == NULL)
                        continue;

                for (node = oldnodes[i]; node; node = noden) {
                        noden = node->next;

                        _coolhash_node_lock(node); /* Just grab the lock to
                                                      wait for the last
                                                      reference to go away. */
                        _coolhash_node_unlock(node);

                        if (node->del) { /* Free this node */
                                pthread_mutex_destroy(&node->node_mx);
                                free(node);
                                continue;
                        }

                        /* Move the node to the new table */
                        _coolhash_table_add(table, node);
                }
        }

        free(oldnodes);
}

/* vim: set et ts=8 sw=8 sts=8: */

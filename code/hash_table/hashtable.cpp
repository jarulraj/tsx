#include <cstdlib>
#include <cstdio>
#include <cstring>

using namespace std;
 
#include "hashtable.h"
#include "murmur.h"
#include "error.h"

uint32_t global_seed = 2976579765;

//-----------------------------------
// HashTable functions
//-----------------------------------

void HashTable::ht_init(HashTable *table, ht_flags flags, double max_load_factor)
{
    table->array_size = HT_INITIAL_SIZE;
    table->array = (HashEntry**) malloc(table->array_size * sizeof(*(table->array)));
    if(table->array == NULL) {
        debug("ht_init failed to allocate memory\n");
    }
    table->key_count = 0;
    table->collisions = 0;
    table->flags = flags;
    table->max_load_factor = max_load_factor;
    table->current_load_factor = 0.0;

    unsigned int i;
    for(i = 0; i < table->array_size; i++)
    {
        table->array[i] = NULL;
    }

    return;
}

void HashTable::ht_destroy(HashTable *table)
{
    unsigned int i;
    HashEntry *entry;
    HashEntry *tmp;

    if(table->array == NULL) {
        debug("ht_destroy got a bad table\n");
    }

    // crawl the entries and delete them
    for(i = 0; i < table->array_size; i++) {
        entry = table->array[i];

        while(entry != NULL) {
            tmp = entry->next;
            entry->he_destroy(table->flags, entry);
            entry = tmp;
        }
    }

    table->array_size = 0;
    table->key_count = 0;
    table->collisions = 0;
    free(table->array);
    table->array = NULL;
}

void HashTable::ht_insert(HashTable *table, void *key, size_t key_size, void *value, size_t value_size)
{
    HashEntry *entry = entry->he_create(table->flags, key, key_size, value, value_size);
    ht_insert_he(table, entry);
}

// this was separated out of the regular ht_insert
// for ease of copying hash entries around
void HashTable::ht_insert_he(HashTable *table, HashEntry *entry){
    HashEntry *tmp;
    unsigned int index;

    entry->next = NULL;
    index = ht_index(table, entry->key, entry->key_size);
    tmp = table->array[index];

    // if true, no collision
    if(tmp == NULL)
    {
        table->array[index] = entry;
        table->key_count++;
        return;
    }

    // walk down the chain until we either hit the end
    // or find an identical key (in which case we replace
    // the value)
    while(tmp->next != NULL)
    {
        if(entry->he_key_compare(tmp, entry))
            break;
        else
            tmp = tmp->next;
    }

    if(entry->he_key_compare(tmp, entry))
    {
        // if the keys are identical, throw away the old entry
        // and stick the new one into the table
        entry->he_set_value(table->flags, tmp, entry->value, entry->value_size);
        entry->he_destroy(table->flags, entry);
    }
    else
    {
        // else tack the new entry onto the end of the chain
        tmp->next = entry;
        table->collisions += 1;
        table->key_count ++;
        table->current_load_factor = (double)table->collisions / table->array_size;

        // double the size of the table if autoresize is on and the
        // load factor has gone too high
        if(!(table->flags & HT_NO_AUTORESIZE) && (table->current_load_factor > table->max_load_factor)) {
            ht_resize(table, table->array_size * 2);
            table->current_load_factor = (double)table->collisions / table->array_size;
        }
    }
}

void* HashTable::ht_get(HashTable *table, void *key, size_t key_size, size_t *value_size)
{
    unsigned int index = ht_index(table, key, key_size);
    HashEntry *entry = table->array[index];
    HashEntry tmp;
    tmp.key = key;
    tmp.key_size = key_size;

    // once we have the right index, walk down the chain (if any)
    // until we find the right key or hit the end
    while(entry != NULL)
    {
        if(entry->he_key_compare(entry, &tmp))
        {
            if(value_size != NULL)
                *value_size = entry->value_size;

            return entry->value;
        }
        else
        {
            entry = entry->next;
        }
    }

    return NULL;
}

void HashTable::ht_remove(HashTable *table, void *key, size_t key_size)
{
    unsigned int index = ht_index(table, key, key_size);
    HashEntry *entry = table->array[index];
    HashEntry *prev = NULL;
    HashEntry tmp;
    tmp.key = key;
    tmp.key_size = key_size;

    // walk down the chain
    while(entry != NULL)
    {
        // if the key matches, take it out and connect its
        // parent and child in its place
        if(entry->he_key_compare(entry, &tmp))
        {
            if(prev == NULL)
                table->array[index] = entry->next;
            else
                prev->next = entry->next;

            table->key_count--;
            if(prev != NULL)
              table->collisions--;
            entry->he_destroy(table->flags, entry);
            return;
        }
        else
        {
            prev = entry;
            entry = entry->next;
        }
    }
}

int HashTable::ht_contains(HashTable *table, void *key, size_t key_size)
{
    unsigned int index = ht_index(table, key, key_size);
    HashEntry *entry = table->array[index];

    HashEntry tmp;
    tmp.key = key;
    tmp.key_size = key_size;

    // walk down the chain, compare keys
    while(entry != NULL)
    {
        if(entry->he_key_compare(entry, &tmp))
            return 1;
        else
            entry = entry->next;
    }

    return 0;
}

unsigned int HashTable::ht_size(HashTable *table)
{
    return table->key_count;
}

void** HashTable::ht_keys(HashTable *table, unsigned int *key_count)
{
    void **ret;

    if(table->key_count == 0){
      *key_count = 0;
      return NULL;
    }

    // array of pointers to keys
    ret = (void**) malloc(table->key_count * sizeof(void *));
    if(ret == NULL) {
        debug("ht_keys failed to allocate memory\n");
    }
    *key_count = 0;

    unsigned int i;
    HashEntry *tmp;

    // loop over all of the chains, walk the chains,
    // add each entry to the array of keys
    for(i = 0; i < table->array_size; i++)
    {
        tmp = table->array[i];

        while(tmp != NULL)
        {
            ret[*key_count]=tmp->key;
            *key_count += 1;
            tmp = tmp->next;
            // sanity check, should never actually happen
            if(*key_count >= table->key_count) {
                debug("ht_keys: too many keys, expected %d, got %d\n", table->key_count, *key_count);
            }
        }
    }

    return ret;
}

void HashTable::ht_clear(HashTable *table)
{
    // TODO :  Fix constructor
    HashTable* tmp = new HashTable;
    if(table != NULL){
        tmp->ht_destroy(table);
        tmp->ht_init(table, (ht_flags) table->flags, table->max_load_factor);
    }
}

unsigned int HashTable::ht_index(HashTable *table, void *key, size_t key_size)
{
    uint32_t index;
    // 32 bits of murmur seems to fare pretty well
    MurmurHash3_x86_32(key, key_size, (uint32_t) global_seed, &index);
    index %= table->array_size;
    return index;
}

// new_size can be smaller than current size (downsizing allowed)
void HashTable::ht_resize(HashTable *table, unsigned int new_size)
{
    HashTable new_table;

    debug("ht_resize(old=%d, new=%d)\n",table->array_size,new_size);
    new_table.array_size = new_size;
    new_table.array = (HashEntry**) malloc(new_size * sizeof(HashEntry*));
    new_table.key_count = 0;
    new_table.collisions = 0;
    new_table.flags = table->flags;
    new_table.max_load_factor = table->max_load_factor;

    unsigned int i;
    for(i = 0; i < new_table.array_size; i++)
    {
        new_table.array[i] = NULL;
    }

    HashEntry *entry;
    HashEntry *next;
    for(i = 0; i < table->array_size; i++)
    {
        entry = table->array[i];
        while(entry != NULL)
        {
            next = entry->next;
            ht_insert_he(&new_table, entry);
            entry = next;
        }
        table->array[i]=NULL;
    }

    ht_destroy(table);

    table->array_size = new_table.array_size;
    table->array = new_table.array;
    table->key_count = new_table.key_count;
    table->collisions = new_table.collisions;

}

void HashTable::ht_set_seed(uint32_t seed){
    global_seed = seed;
}

//---------------------------------
// HashEntry functions
//---------------------------------

HashEntry* HashEntry::he_create(int flags, void *key, size_t key_size, void *value, size_t value_size)
{
    HashEntry *entry = (HashEntry*) malloc(sizeof(*entry));
    if(entry == NULL) {
        debug("Failed to create HashEntry\n");
        return NULL;
    }

    entry->key_size = key_size;
    if (flags & HT_KEY_CONST){
        entry->key = key;
    }
    else {
        entry->key = malloc(key_size);
        if(entry->key == NULL) {
            debug("Failed to create HashEntry\n");
            free(entry);
            return NULL;
        }
        memcpy(entry->key, key, key_size);
    }

    entry->value_size = value_size;
    if (flags & HT_VALUE_CONST){
        entry->value = value;
    }
    else {
        entry->value = malloc(value_size);
        if(entry->value == NULL) {
            debug("Failed to create HashEntry\n");
            free(entry->key);
            free(entry);
            return NULL;
        }
        memcpy(entry->value, value, value_size);
    }

    entry->next = NULL;

    return entry;
}

void HashEntry::he_destroy(int flags, HashEntry *entry)
{
    if (!(flags & HT_KEY_CONST))
        free(entry->key);
    if (!(flags & HT_VALUE_CONST))
        free(entry->value);
    free(entry);
}

int HashEntry::he_key_compare(HashEntry *e1, HashEntry *e2)
{
    char *k1 = (char*) e1->key;
    char *k2 = (char*) e2->key;

    if(e1->key_size != e2->key_size)
        return 0;

    return (memcmp(k1,k2,e1->key_size) == 0);
}

void HashEntry::he_set_value(int flags, HashEntry *entry, void *value, size_t value_size)
{
    if (!(flags & HT_VALUE_CONST)){
        if(entry->value)
            free(entry->value);

        entry->value = malloc(value_size);
        if(entry->value == NULL) {
            debug("Failed to set entry value\n");
            return;
        }
        memcpy(entry->value, value, value_size);
    } else {
        entry->value = value;
    }
    entry->value_size = value_size;

    return;
}


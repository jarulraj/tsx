#ifndef HashTable_H
#define HashTable_H

#include <stdint.h>
#include <stddef.h>

/// The initial size of the hash table.
#define HT_INITIAL_SIZE 64
 
#include <string.h>
#include <stdint.h>

using namespace std;

typedef uint64_t uint64;

class HashTable;

class HashEntry {

    void *key;
    void *value;
    size_t key_size;
    size_t value_size;
    struct HashEntry *next;

    public:

    HashEntry *he_create(int flags, void *key, size_t key_size, void *value, size_t value_size);

    void he_destroy(int flags, HashEntry *entry);

    /// This is a "deep" compare, rather than just comparing pointers.
    int he_key_compare(HashEntry *e1, HashEntry *e2);

    /// Sets the value on an existing hash entry.
    void he_set_value(int flags, HashEntry *entry, void *value, size_t value_size);

    friend class HashTable ; 
} ;
 
/// Hashtable initialization flags (passed to ht_init)
typedef enum {
    /// No options set
    HT_NONE = 0,
    /// Constant length key, useful if your keys are going to be a fixed size.
    HT_KEY_CONST = 1,
    /// Constant length value.
    HT_VALUE_CONST = 2,
    /// Don't automatically resize hashtable when the load factor
    /// goes above the trigger value
    HT_NO_AUTORESIZE = 4
} ht_flags;
 
class HashTable {

    private:    
        HashEntry **array;
        unsigned int key_count;
        unsigned int array_size;
        unsigned int collisions;
        int flags;

        /// load_factor is the ratio of collisions to table size
        double current_load_factor;
        double max_load_factor;

    public:
        // For example: if max_load_factor = 0.1, the table will resize if the number
        // of collisions increases beyond 1/10th of the size of the table
        void ht_init(HashTable *table, ht_flags flags, double max_load_factor);

        void ht_destroy(HashTable *table);

        void ht_insert(HashTable *table, void *key, size_t key_size, void *value, size_t value_size);

        void ht_insert_he(HashTable *table, HashEntry *entry);

        /// @brief Returns a pointer to the value with the matching key,
        ///         value_size is set to the size in bytes of the value
        void* ht_get(HashTable *table, void *key, size_t key_size, size_t *value_size);
        void ht_remove(HashTable *table, void *key, size_t key_size);
        int ht_contains(HashTable *table, void *key, size_t key_size);

        unsigned int ht_size(HashTable *table);
        void** ht_keys(HashTable *table, unsigned int *key_count);
        void ht_clear(HashTable *table);

        /// @brief Calulates the index in the hash table's internal array
        ///        from the given key (used for debugging currently).
        unsigned int ht_index(HashTable *table, void *key, size_t key_size);
        void ht_resize(HashTable *table, unsigned int new_size);

        /// @brief Sets the global security seed to be used in hash function.
        void ht_set_seed(uint32_t seed);

} ;


#endif


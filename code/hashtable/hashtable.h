#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

using namespace std;

#define HT_INITIAL_SIZE 64  // Initial HT size

// HT initialization flags
typedef enum {
    HT_NONE = 0,            // no options 
    HT_KEY_CONST = 1,       // const length key
    HT_VALUE_CONST = 2,     // const length value
    HT_NO_AUTORESIZE = 4    // no automatic resize 
} ht_flags;

class HashEntry {
    private:

        void *key;
        void *value;
        size_t key_size;
        size_t value_size;
        int flags;

        HashEntry *next;

    public:
        void InitHashEntry(int flags, void *key, size_t key_size, void *value, size_t value_size);
        
        HashEntry();

        HashEntry(int flags, void *key, size_t key_size, void *value, size_t value_size);

        void Destroy();

        /// This is a "deep" compare, rather than just comparing pointers.
        bool KeyCompare(HashEntry *e1, HashEntry *e2);

        /// Sets the value on an existing hash entry.
        void SetValue(int flags, void *value, size_t value_size);

        friend class HashTable ; 
};


class HashTable {
    private:    
        HashEntry **array;

        unsigned int key_count;
        unsigned int array_size;
        unsigned int collisions;

        double current_load_factor; // load_factor = ratio of collisions to table size
        double max_load_factor;

        int flags;

    public:
        
        HashTable(ht_flags flags = (ht_flags)HT_NONE, double max_load_factor = 0.05);
        ~HashTable() { Destroy(); }

        void Destroy();

        void Insert(void *key, size_t key_size, void *value, size_t value_size);

        void InsertEntry(HashEntry *entry);

        void* Get(void *key, size_t key_size, size_t *value_size);

        void Remove(void *key, size_t key_size);

        bool HasKey(void *key, size_t key_size);

        unsigned int GetSize();

        void** GetKeys(unsigned int *key_count);

        void Clear();

        unsigned int GetIndex(void *key, size_t key_size);

        void Resize(unsigned int new_size);

        void SetSeed(uint32_t seed);  // global seed for hash function
};

#endif

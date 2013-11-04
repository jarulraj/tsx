#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>

using namespace std;
 
#include "hashtable.h"
#include "murmur.h"
#include "error.h"

uint32_t global_seed = 2976579765;
double default_max_load_factor = 0.05 ;

//-----------------------------------
// HashTable functions
//-----------------------------------
 
HashTable::HashTable(ht_flags flags, double max_load_factor) {
    array_size = HT_INITIAL_SIZE;
    array = (HashEntry**) malloc(array_size * sizeof(*(array)));

    if(array == NULL) {
        debug("ht_init failed to allocate memory\n");
    }

    key_count = 0;
    collisions = 0;
    this->flags = flags  ;
    this->max_load_factor = max_load_factor;
    current_load_factor = 0.0;

    unsigned int i;
    for(i = 0; i < array_size; i++){
        array[i] = NULL;
    }
}

void HashTable::Destroy()
{
    unsigned int i;
    HashEntry *entry;
    HashEntry *tmp;

    if(array == NULL) {
        debug("HT destructor got a bad table\n");
    }

    // traverse all entries and delete
    for(i = 0; i < array_size; i++) {
        entry = array[i];

        while(entry != NULL) {
            tmp = entry->next;
            delete entry;
            entry = tmp;
        }
    }

    array_size = 0;
    key_count = 0;
    collisions = 0;
    free(array);  
    array = NULL;
}

void HashTable::HT_Insert(void *key, size_t key_size, void *value, size_t value_size)
{
    // Use HT flags
    HashEntry* entry = new HashEntry(this->flags, key, key_size, value, value_size);

    InsertEntry(entry);
}

// Separated out of the regular ht_insert for ease of copying hash entries around
void HashTable::InsertEntry(HashEntry *entry){
    HashEntry *tmp;
    unsigned int index;

    entry->next = NULL;
    index = GetIndex(entry->key, entry->key_size);
    tmp = array[index];

    // if true, no collision
    if(tmp == NULL)
    {
        array[index] = entry;
        key_count++;
        return;
    }

    // walk down the chain until we either hit the end
    // or find an identical key (in which case we replace
    // the value)
    while(tmp->next != NULL)
    {
        if(entry->KeyCompare(tmp, entry))
            break;
        else
            tmp = tmp->next;
    }

    if(entry->KeyCompare(tmp, entry))
    {
        // if the keys are identical, throw away the old entry
        // and stick the new one into the table
        tmp->SetValue(flags, entry->value, entry->value_size);
    }
    else
    {
        // else tack the new entry onto the end of the chain
        tmp->next = entry;
        collisions += 1;
        key_count ++;
        current_load_factor = (double)collisions / array_size;

        // double the size of the table if autoresize is on and the
        // load factor has gone too high
        if(!(flags & HT_NO_AUTORESIZE) && (current_load_factor > max_load_factor)) {
            debug("Current load factor : %lf Max : %lf \n", current_load_factor, max_load_factor);
            Resize(array_size * 2);
            current_load_factor = (double)collisions / array_size;
        }
    }
}

void* HashTable::HT_Get(void *key, size_t key_size, size_t *value_size)
{
    unsigned int index = GetIndex(key, key_size);
    HashEntry *entry = array[index];
    HashEntry *tmp = new HashEntry;
    tmp->key = key;
    tmp->key_size = key_size;

    // once we have the right index, walk down the chain (if any)
    // until we find the right key or hit the end
    while(entry != NULL)
    {
        if(entry->KeyCompare(entry,tmp))
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

void HashTable::HT_Remove(void *key, size_t key_size)
{
    unsigned int index = GetIndex(key, key_size);
    HashEntry *entry = array[index];
    HashEntry *prev = NULL;
    HashEntry *tmp = new HashEntry;
    tmp->key = key;
    tmp->key_size = key_size;

    // walk down the chain
    while(entry != NULL)
    {
        // if the key matches, take it out and connect its
        // parent and child in its place
        if(entry->KeyCompare(entry, tmp))
        {
            if(prev == NULL)
                array[index] = entry->next;
            else
                prev->next = entry->next;

            key_count--;
            if(prev != NULL)
                collisions--;

            entry->Destroy();
            return;
        }
        else
        {
            prev = entry;
            entry = entry->next;
        }
    }
}

bool HashTable::HasKey(void *key, size_t key_size)
{
    unsigned int index = GetIndex(key, key_size);
    HashEntry *entry = array[index];

    HashEntry *tmp = new HashEntry;
    tmp->key = key;
    tmp->key_size = key_size;

    // walk down the chain, compare keys
    while(entry != NULL)
    {
        if(entry->KeyCompare(entry, tmp))
            return true;
        else
            entry = entry->next;
    }

    return false;
}

/** Wrappers **/
void HashTable::Insert(uint64_t key, const std::string &value){
    char* HT_key;
    char* HT_value;

    HT_key = new char[21]; // Max size for uint64_t
    sprintf(HT_key, "%ld", key);

    HT_value = new char[value.length() + 1];
    strcpy(HT_value,value.c_str());

    HT_Insert(HT_key, strlen(HT_key), HT_value, strlen(HT_value));

    //free(HT_value);
}

void HashTable::Delete(uint64_t key){
    char* HT_key;
    
    HT_key = new char[21]; 
    sprintf(HT_key, "%ld", key);
    
    HT_Remove(HT_key, strlen(HT_key));
}

const bool HashTable::Get(uint64_t key, std::string *result){
    char* HT_key;
    char* HT_value;
    size_t HT_value_size;

    HT_key = new char[21]; 
    sprintf(HT_key, "%ld", key);
    
    if(!HasKey(HT_key,strlen(HT_key))){
        std::string HT_value_string("NULL");
        *result = HT_value_string;
        return false;
    }
    else{
        HT_value = (char*) HT_Get(HT_key, strlen(HT_key), &HT_value_size);
        std::string HT_value_string(HT_value, HT_value + HT_value_size);
        *result = HT_value_string;
        return true;
    }
}

void HashTable::display(){
    if(this->key_count == 0){
        printf("Empty Map \n");
        return;
    }

    void** ret;
    unsigned int itr = 0;

    // array of pointers to keys
    ret = (void**) malloc(this->key_count * sizeof(void *));
    if(ret == NULL) {
        debug("GetKeys failed to allocate memory\n");
    }

    unsigned int i;
    HashEntry *tmp;

    printf("---------------------------------\n");
    printf("Num Buckets :: %d \n", array_size);
    
    // loop over all of the chains, walk the chains,
    // add each entry to the array of keys
    for(i = 0; i < array_size; i++)
    {
        tmp = array[i];

        if(tmp != NULL)
            printf("Bucket %d :: \n", i);

        while(tmp != NULL)
        {
            ret[itr]=tmp->key;
           
            printf("[ %s : %s ]\n", (char*)tmp->key, (char*)tmp->value);
            //printf("[ %d : %d ]\n", *((int*)tmp->key), *((int*)tmp->value)); # FOR <int*, int*> Hashtables
            
            itr += 1;
            tmp = tmp->next;
            // sanity check, should never actually happen
            if(itr >= this->key_count) {
                debug("GetKeys : too many keys, expected %d, got %d\n", this->key_count, itr);
            }
        }

    }
    printf("---------------------------------\n");
}

unsigned int HashTable::GetSize()
{
    return key_count;
}

void** HashTable::GetKeys(unsigned int *key_count)
{
    void **ret;

    if(this->key_count == 0){
        *key_count = 0;
        return NULL;
    }

    // array of pointers to keys
    ret = (void**) malloc(this->key_count * sizeof(void *));
    if(ret == NULL) {
        debug("GetKeys failed to allocate memory\n");
    }
    *key_count = 0;

    unsigned int i;
    HashEntry *tmp;

    // loop over all of the chains, walk the chains,
    // add each entry to the array of keys
    for(i = 0; i < array_size; i++)
    {
        tmp = array[i];

        while(tmp != NULL)
        {
            ret[*key_count]=tmp->key;
            *key_count += 1;
            tmp = tmp->next;
            // sanity check, should never actually happen
            if(*key_count >= this->key_count) {
                debug("GetKeys : too many keys, expected %d, got %d\n", this->key_count, *key_count);
            }
        }
    }

    return ret;
}

void HashTable::Clear()
{
    HashTable* new_table = new HashTable((ht_flags) this->flags, this->max_load_factor);

    this->Destroy();

    array_size = new_table->array_size;
    array = new_table->array;             // Copies bucket array
    key_count = new_table->key_count;
    collisions = new_table->collisions;
    flags = new_table->flags;
    max_load_factor = new_table->max_load_factor;

}

unsigned int HashTable::GetIndex(void *key, size_t key_size)
{
    uint32_t index;
    // 32 bits of murmur seems to fare pretty well
    MurmurHash3_x86_32(key, key_size, (uint32_t) global_seed, &index);
    index %= array_size;
    return index;
}

// new_size can be smaller than current size (downsizing allowed)
void HashTable::Resize(unsigned int new_size)
{
    HashTable *new_table =  new HashTable;

    debug("ht_resize(old=%d, new=%d)\n",array_size,new_size);
    new_table->array_size = new_size;
    new_table->array = (HashEntry**) malloc(new_size * sizeof(HashEntry*));
    new_table->key_count = 0;
    new_table->collisions = 0;
    new_table->flags = this->flags;
    new_table->max_load_factor = this->max_load_factor;

    unsigned int i;
    for(i = 0; i < new_table->array_size; i++)
    {
        new_table->array[i] = NULL;
    }

    HashEntry *entry;
    HashEntry *next;
    for(i = 0; i < array_size; i++)
    {
        entry = array[i];
        while(entry != NULL)
        {
            next = entry->next;
            new_table->InsertEntry(entry);
            entry = next;
        }
        array[i]=NULL;
    }

    this->Destroy();

    array_size = new_table->array_size;
    array = new_table->array;             // Copies bucket array
    key_count = new_table->key_count;
    collisions = new_table->collisions;
    flags = new_table->flags;
    max_load_factor = new_table->max_load_factor;

}

void HashTable::SetSeed(uint32_t seed){
    global_seed = seed;
}

//---------------------------------
// HashEntry functions
//---------------------------------

void HashEntry::InitHashEntry(int flags, void *key, size_t key_size, void *value, size_t value_size){
    this->key_size = key_size;
    if (flags & HT_KEY_CONST){
        this->key = key;
    }
    else {
        // TODO : Replace malloc and free ?
        this->key = malloc(key_size);
        if(this->key == NULL) {
            debug("Failed to create HashEntry\n");
        }
        memcpy(this->key, key, key_size);
    }

    this->value_size = value_size;
    if (flags & HT_VALUE_CONST){
        this->value = value;
    }
    else {
        this->value = malloc(value_size);
        if(this->value == NULL) {
            debug("Failed to create HashEntry\n");
            free(this->key);
        }
        memcpy(this->value, value, value_size);
    }

    this->next = NULL;
    this->flags = flags; 
}

HashEntry::HashEntry()
{
    // TODO : Convention
    int key = 0;
    char value = 'X' ;

    InitHashEntry(ht_flags(HT_NONE), (void*) &key , sizeof(key), (void*) &value, sizeof(value));
}

HashEntry::HashEntry(int flags, void *key, size_t key_size, void *value, size_t value_size)
{
    InitHashEntry(flags, key, key_size, value, value_size);
}

void HashEntry::Destroy()
{
    if (!(this->flags & HT_KEY_CONST))
        free(this->key);
    if (!(this->flags & HT_VALUE_CONST))
        free(this->value);
}

bool HashEntry::KeyCompare(HashEntry* e1, HashEntry* e2)
{
    char *k1 = (char*) e1->key;
    char *k2 = (char*) e2->key;

    if(e1->key_size != e2->key_size)
        return false;

    return (memcmp(k1,k2,e1->key_size) == 0);
}

void HashEntry::SetValue(int flags, void *value, size_t value_size)
{
    if (!(flags & HT_VALUE_CONST)){
        if(this->value)
            free(this->value);

        this->value = malloc(value_size);
        if(this->value == NULL) {
            debug("Failed to set entry value\n");
            return;
        }
        memcpy(this->value, value, value_size);
    } else {
        this->value = value;
    }

    this->value_size = value_size;
    this->flags = flags;
}


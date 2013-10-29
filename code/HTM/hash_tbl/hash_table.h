/*
 * hash_tbl.h
 *
 *  Created on: Oct 28, 2013
 *      Author: jdunietz
 */

#ifndef HASH_TABLE_H_
#define HASH_TABLE_H_

#include <string>
#include <cstdint>

typedef u_int64_t uint64;

class HashTable {
public:
    void Insert(uint64 key, const std::string &value);
    void Delete(uint64 key);
    const bool Get(uint64 key, std::string *result);
};


#endif /* HASH_TABLE_H_ */

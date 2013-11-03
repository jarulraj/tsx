#ifndef TXNMANAGERS_H_
#define TXNMANAGERS_H_

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "hashtable.h"

using namespace std;

typedef enum OpType {
    INSERT,
    GET,
    DELETE
} OpType;

typedef struct OpDescription {
    OpType type;
    uint64_t key;
    std::string value;
} OpDescription;


class TxnManager {
public:
    TxnManager(HashTable *table) : table_(table) {}

    virtual ~TxnManager() {
        delete table_;
    }

    // Does whatever transaction initialization is needed, calls ExecuteTxnOps
    // to actually perform the operations, then does any necessary transaction
    // finalization/cleanup.  If get_results is non-null, it contains the
    // results of all GET operations when the function returns.
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results) = 0;

protected:
    // Returns false iff the operations were semantically ill-formed (for now,
    // this just means there were GETs or DELETEs on non-existent keys).
    virtual bool ExecuteTxnOps(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    HashTable *table_;
};


class LockTableTxnManager : public TxnManager {
public:
    LockTableTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    unordered_map<uint64_t, mutex*> lockTable;
    // Prevents concurrent insertions to the lock table.
    mutex tableMutex;
};


class SpinLockTxnManager : public TxnManager {
public:
    SpinLockTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    unordered_map<uint64_t, atomic_flag*> lockTable;
    // Prevents concurrent insertions to the lock table.
    mutex tableMutex;
};


class HTMTxnManager : public TxnManager {
public:
    HTMTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);
};

#endif /* TXNMANAGERS_H_ */

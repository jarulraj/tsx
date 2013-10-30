#ifndef TXNMANAGERS_H_
#define TXNMANAGERS_H_

#include <vector>
#include <string>

#include "hashtable.h"

using namespace std;

typedef enum OpType {
    INSERT,
    GET,
    DELETE
} OpType;

typedef struct OpDescription {
    OpType type;
    uint64 key;
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
    // finalization/cleanup.
    virtual bool RunTxn(const std::vector<OpDescription> &operations) = 0;

protected:
    // Returns false iff the operations were semantically ill-formed (for now,
    // this just means there were GETs on non-existent keys).
    virtual bool ExecuteTxnOps(const std::vector<OpDescription> &operations);

private:
    HashTable *table_;
};


class PessimisticTxnManager : public TxnManager {
public:
    PessimisticTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations);
};


class HTMTxnManager : public TxnManager {
public:
    HTMTxnManager(HashTable *table) : TxnManager(table) {}
    // TODO: Implement me
    virtual bool RunTxn(const std::vector<OpDescription> &operations);
};

#endif /* TXNMANAGERS_H_ */

#ifndef _HLE_TXN_MANAGER_H_
#define _HLE_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "tsx.h"

class HLETxnManager : public TxnManager {
public:
    HLETxnManager(std::unordered_map<long,std::string> *table);

    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

    void getStats();

private:
    static spinlock_t table_lock;
};


#endif /* _HLE_TXN_MANAGER_H_ */

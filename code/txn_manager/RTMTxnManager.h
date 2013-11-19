#ifndef _RTM_TXN_MANAGER_H_
#define _RTM_TXN_MANAGER_H_

#include <iostream>
#include <set>

#include "TxnManager.h"       
#include "tsx.h"

class RTMTxnManager : public TxnManager {
public:
    RTMTxnManager(std::unordered_map<long,std::string> *table);

    virtual bool RunTxn(const std::vector<OpDescription> &operations,
            std::vector<string> *get_results);

private:
    static spinlock_t table_lock;
};


#endif /* _RTM_TXN_MANAGER_H_ */

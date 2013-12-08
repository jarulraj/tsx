#include <iostream>
#include <set>
#include <unordered_map>
#include <utility> 
#include <chrono>
#include <thread>

#include "TxnManager.h"
#include "tester/generators.h"

using namespace std;
using namespace std::chrono;

bool TxnManager::ExecuteTxnOps(const vector<OpDescription> &operations,
        std::vector<string> *get_results) {
    string result;
    for (const OpDescription &op : operations) {
        if (op.type == GET && get_results != NULL) {
            ExecuteTxnOp(op, &result);
            get_results->push_back(result);
        } else {
            ExecuteTxnOp(op);
        }
    }

    return true;
}

// UNORDERED MAP
void TxnManager::ExecuteTxnOp(const OpDescription &op, string *result) {
    if (op.type == INSERT) {
        if (result) {
            *result = (*table_)[op.key];
        }

        // Overwrite value if already present.
        (*table_)[op.key] = op.value;
    }
    else if (op.type == GET) {
        //at function throws an out_of_range exception if key not found
        string get_result = (*table_)[op.key];

        if(result){
            *result = get_result;
        }
    }
    else { // op.type == DELETE
        if (result) {
            *result = table_->at(op.key);
        }

        table_->erase(op.key);
    }
}

// CUSTOM HASHTABLE
/*
   void TxnManager::ExecuteTxnOp(const OpDescription &op, string *result) {
   if (op.type == INSERT) {
   if (result) {
   table_->Get(op.key, result);
   }

   table_->Insert(op.key, op.value);
   }
   else if (op.type == GET) {
   string get_result;
   table_->Get(op.key, &get_result);
   if (result) {
 *result = get_result;
 }
 }
 else { // op.type == DELETE
 if (result) {
 table_->Get(op.key, result);
 }

 table_->Delete(op.key);
 }
 }
 */

void TxnManager::printStats(){
    cout<<"TxnManager Stats"<<endl;
}


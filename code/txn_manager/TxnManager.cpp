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

#define OP_COUNT 1000000

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

    // SIMULATING TEMPORAL OVERHEAD OF APPLICATION LOGIC
    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    int dummy_ops = OP_COUNT;
    int val = rand() % 10;
    for(int op_itr = 0 ; op_itr++ ; op_itr < dummy_ops)
        val *= 2; 

    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    nanoseconds gap = duration_cast<nanoseconds>(t2 - t1);
    //cout<<"Gap :"<<gap.count()<<" ns"<<endl;

    return true;
}

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
        const string &get_result = table_->at(op.key);
        if (result) {
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

void TxnManager::printStats(){
    cout<<"TxnManager Stats"<<endl;
}


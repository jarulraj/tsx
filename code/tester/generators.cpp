#include <iostream>

#include "cmdline-utils.h"
#include "generators.h"

using namespace std;

const int &UniformGenerator::nextElement() {
    lastElement = distribution(generator);
    return lastElement;
}

const int &ZipfianGenerator::nextElement() {
    if (num_items != count_for_zeta) {

        //have to recompute zetan and eta, since they depend on num_items
        if (num_items > count_for_zeta) {
            //we have added more items. can compute zetan incrementally, which is cheaper
            zetan = zeta(count_for_zeta, num_items, theta, zetan);
            eta = (1 - pow(2.0 / num_items, 1 - theta))
                    / (1 - zeta2theta / zetan);
        } else if ((num_items < count_for_zeta)
                && (allow_item_count_decrease)) {
            //have to start over with zetan
            //note : for large itemsets, this is very slow. so don't do it!
            cout << "WARNING: Recomputing Zipfian distribution. This is slow"
                    << " and should be avoided. (num_items=" << num_items
                    << ", count_for_zeta=" << count_for_zeta << ")";
            zetan = zeta(num_items, theta);
            eta = (1 - pow(2.0 / num_items, 1 - theta))
                    / (1 - zeta2theta / zetan);
        }
    }

    double u = distribution(generator);
    double uz = u * zetan;

    if (uz < 1.0) {
        static const int ZERO = 0;
        return ZERO;
    }

    if (uz < 1.0 + pow(0.5, theta)) {
        static const int ONE = 1;
        return ONE;
    }

    lastElement = min_item + (int) ((num_items) * pow(eta * u - eta + 1, alpha));
    return lastElement;
}

#include <iostream>

#include "cmdline-utils.h"
#include "generators.h"

using namespace std;

const long &UniformGenerator::nextElement() {
    lastElement = distribution(generator);
    return lastElement;
}

const long &ZipfianGenerator::nextElement() {
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
        return ZERO;
    }

    if (uz < 1.0 + pow(0.5, theta)) {
        return ONE;
    }

    long next = min_item + static_cast<int>( ((num_items) * pow(eta * u - eta + 1, alpha)) );
    next = ScrambleIfNeeded(next);
    lastElement = next;
    return lastElement;
}

HotSpotGenerator::HotSpotGenerator(int lower_bound, int upper_bound,
        double hotset_fraction, double hot_opn_fraction)
        : generator(time(NULL)) {
    if (hotset_fraction < 0.0 || hotset_fraction > 1.0) {
        cerr << "Hotset fraction out of range. Setting to 0.0" << endl;
        hotset_fraction = 0.0;
    }
    if (hot_opn_fraction < 0.0 || hot_opn_fraction > 1.0) {
        cerr << "Hot operation fraction out of range. Setting to 0.0" << endl;
        hot_opn_fraction = 0.0;
    }
    if (lower_bound > upper_bound) {
        cerr << "Upper bound of HotSpot generator smaller than the lower bound. "
             << "Swapping the values." << endl;
        int temp = lower_bound;
        lower_bound = upper_bound;
        upper_bound = temp;
    }

    this->lower_bound = lower_bound;
    this->upper_bound = upper_bound;
    this->hotset_fraction = hotset_fraction;

    int interval = upper_bound - lower_bound + 1;
    this->hot_interval = static_cast<int>(interval * hotset_fraction);
    this->cold_interval = interval - hot_interval;
    this->hotset_opn_fraction = hot_opn_fraction;
}

const long &HotSpotGenerator::nextElement() {
    uniform_real_distribution<double> doubles;
    if (doubles(generator) < hotset_opn_fraction) {
      // Choose a value from the hot set.
        uniform_int_distribution<int> ints(0, hot_interval);
        lastElement = lower_bound + ints(generator);
    } else {
      // Choose a value from the cold set.
        uniform_int_distribution<int> ints(0, cold_interval);
        lastElement = lower_bound + hot_interval + ints(generator);
    }

    return lastElement;
}

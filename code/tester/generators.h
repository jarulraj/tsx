#ifndef GENERATORS_H_
#define GENERATORS_H_

#include <ctime>
#include <functional>
#include <random>
#include <string>

template <typename T>
class Generator {
public:
    virtual const T &nextElement() = 0;
    const T &getLastElement() const {return lastElement;}
    virtual ~Generator() {}
protected:
    T lastElement;
};

class UniformGenerator : public Generator<long> {
public:
    UniformGenerator(long min, long max)
        : generator(time(NULL)), distribution(min, max) {}
    virtual const long &nextElement();
protected:
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
};

// This class has been ported almost in its entirety from the YCSB Java codebase.
class ZipfianGenerator: public Generator<long> {
public:
    ZipfianGenerator(long min, long max, time_t rand_seed, bool scramble = true,
            double zipfian_const = 0.99, double zetan_init = nan(""),
            bool allow_decrease = false)
        : generator(rand_seed), distribution(0.0, 1.0),
          num_items(max - min + 1), min_item(min),
          zipfian_constant(zipfian_const), theta(zipfian_const),
          alpha(1.0/(1.0-zipfian_const)), count_for_zeta(num_items),
          eta((1-pow(2.0/num_items,1-theta))/(1-zeta2theta/zetan)),
          allow_item_count_decrease(allow_decrease),
          scrambled(scramble), scramble_offset(rand_seed),
          ZERO(ScrambleIfNeeded(0)), ONE(ScrambleIfNeeded(1)) {
        zeta2theta = zeta(2, theta);
        if (std::isnan(zetan_init)) {
            zetan = ZetaStatic(max - min + 1, zipfian_const);
        } else {
            zetan = zetan_init;
        }

        nextElement();
    }

    virtual const long &nextElement();

protected:
    inline double zeta(long n, double theta) {
        count_for_zeta = n;
        return ZetaStatic(n, theta);
    }

    inline double zeta(long st, long n, double theta, double initial_sum) {
        count_for_zeta = n;
        return ZetaStatic(st, n, theta, initial_sum);
    }

    static inline double ZetaStatic(long n, double theta) {
        return ZetaStatic(0, n, theta, 0);
    }

    static inline double ZetaStatic(long st, long n, double theta, double initial_sum) {
        double sum = initial_sum;
        for (long i = st; i < n; i++) {
            sum += 1 / (pow(i + 1, theta));
        }
        return sum;
    }

    inline long ScrambleIfNeeded(const long &element) {
        if (scrambled) {
            return min_item + ((element + scramble_offset) % num_items);
        } else {
            return element;
        }
    }

    // If this generator is scrambled, these may not actually be 0
    // and 1 -- they're whatever those numbers are getting mapped to.
    const long ZERO;
    const long ONE;

    long num_items;
    long min_item;
    double zipfian_constant;
    // Computed parameters for generating the distribution.
    double alpha, zetan, eta, theta, zeta2theta;
    // The number of items used to compute zetan the last time.
    long count_for_zeta;
    bool allow_item_count_decrease;
    bool scrambled;
    int scramble_offset;

    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution;
};

class HotSpotGenerator : public Generator<long> {
public:
    HotSpotGenerator(int lower_bound, int upper_bound,
            double hotset_fraction, double hot_opn_fraction);

    virtual const long &nextElement();

protected:
    int lower_bound;
    int upper_bound;
    double hotset_fraction;
    double hotset_opn_fraction;
    int hot_interval;
    int cold_interval;

    std::default_random_engine generator;
};


#endif /* GENERATORS_H_ */

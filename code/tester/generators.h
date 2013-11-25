#ifndef GENERATORS_H_
#define GENERATORS_H_

#include <ctime>
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

class UniformGenerator : public Generator<int> {
public:
    UniformGenerator(int min, int max)
        : generator(time(NULL)), distribution(min, max) {}
    virtual const int &nextElement();
protected:
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
};

// This class has been ported almost in its entirety from the YCSB Java codebase.
class ZipfianGenerator: public Generator<int> {
public:
    ZipfianGenerator(long min, long max, double zipfian_const = 0.99,
            double zetan_init = nan(""),
            bool allow_decrease = false)
        : generator(time(NULL)), distribution(0.0, 1.0),
          num_items(max - min + 1), min_item(min),
          zipfian_constant(zipfian_const), theta(zipfian_const),
          alpha(1.0/(1.0-zipfian_const)), count_for_zeta(num_items),
          eta((1-pow(2.0/num_items,1-theta))/(1-zeta2theta/zetan)),
          allow_item_count_decrease(allow_decrease) {
        zeta2theta = zeta(2, theta);
        if (std::isnan(zetan_init)) {
            zetan = ZetaStatic(max - min + 1, zipfian_const);
        } else {
            zetan = zetan_init;
        }

        nextElement();
    }
    virtual const int &nextElement();

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

    long num_items;
    long min_item;
    double zipfian_constant;
    // Computed parameters for generating the distribution.
    double alpha, zetan, eta, theta, zeta2theta;
    // The number of items used to compute zetan the last time.
    long count_for_zeta;
    bool allow_item_count_decrease;

    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution;
};


#endif /* GENERATORS_H_ */

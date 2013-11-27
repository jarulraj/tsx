#include "tsx.h"
#include "immintrin.h"  /* for _mm_pause() */
#include <iostream>

using namespace std;


int cpu_has_rtm(void)
{
    if (__get_cpuid_max(0, NULL) >= 7) {
        unsigned a, b, c, d;
        __cpuid_count(7, 0, a, b, c, d);
        return !!(b & CPUID_RTM);
    }
    return 0;
}

int cpu_has_hle(void)
{
    if (__get_cpuid_max(0, NULL) >= 7) {
        unsigned a, b, c, d;
        __cpuid_count(7, 0, a, b, c, d);
        return !!(b & CPUID_HLE);
    }
    return 0;
}


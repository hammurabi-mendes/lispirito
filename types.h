#ifndef TYPES_H
#define TYPES_H

#ifdef TARGET_6502
    #include <fixed_point.h>

    using Integral = int32_t;
    using Real = FixedPoint<22, 10>;

    // Make this be floor(log_{10}(2^x)) where x is the number of decimal points
    constexpr int DECIMAL_RESOLUTION = 3;
#else
    using Integral = long;
    using Real = double;
#endif /* TARGET_6502 */

using CounterType = unsigned int;

#endif /* TYPES_H */
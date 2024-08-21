#ifndef TYPES_H
#define TYPES_H

#ifdef TARGET_6502
    #include <fixed_point.h>

    using Integral = int;
    using Real = FixedPoint<16, 8>;
#else
    using Integral = long;
    using Real = double;
#endif /* TARGET_6502 */

#endif /* TYPES_H */
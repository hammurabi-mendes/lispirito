#ifndef TYPES_H
#define TYPES_H

#ifdef TARGET_6502
    #include <fixed_point.h>

    using Integral = long;
    using Real = FixedPoint<22, 10>;
#else
    using Integral = long;
    using Real = double;
#endif /* TARGET_6502 */

#endif /* TYPES_H */
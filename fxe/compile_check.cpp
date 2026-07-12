// The proof, such as it is at this stage: every declaration fxe has recovered from db/
// must be valid C++ and must agree with the sizes db/ claims.
//
// fx_lib proves a FORMAT is understood by round-tripping its bytes. The executable-level
// analogue starts here — a header that does not compile is not a reconstruction of
// anything, and a scalar that is not the width db/ says it is would miscompile every
// access the port makes through it. The static_asserts live in the generated header; this
// translation unit is what makes the compiler evaluate them.
//
// There is nothing to run yet. That is the point: declarations first, then the runtime.

#include "generated/fxe.hpp"

#pragma once

// Macro to detect strictly gcc. __GNUC__ and __GNUG__ were intended to
// indicate the GNU compilers. However, they're also defined by Clang/LLVM and
// Intel compilers to indicate compatibility. This macro can be used to detect
// strictly gcc and not clang or icc.
#if (defined(__GNUC__) || defined(__GNUG__)) && !(defined(__clang__) || defined(__INTEL_COMPILER))
  #define STRICT_GNUC
#endif

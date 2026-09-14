#pragma once

// =====================================================
// Generic macro utilities (reusable anywhere)
// =====================================================

#define ATLASNET_EXPAND(x) x

#define ATLASNET_CAT(a, b) ATLASNET_CAT_I(a, b)
#define ATLASNET_CAT_I(a, b) a##b
#define ATLASNET_CAT2(a, b) ATLASNET_CAT(a, b)
#define ATLASNET_CAT3(a, b, c) ATLASNET_CAT(ATLASNET_CAT(a, b), c)
#define ATLASNET_CAT4(a, b, c, d) ATLASNET_CAT(ATLASNET_CAT3(a, b, c), d)

// -----------------------------------------------------
// Variadic argument count (up to 8 args)
// -----------------------------------------------------
#define ATLASNET_NARGS(...) \
  ATLASNET_NARGS_I(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define ATLASNET_NARGS_I(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N

// -----------------------------------------------------
// FOR_EACH with separator strategy
// Usage:
//   ATLASNET_FOR_EACH(M, SEP, a, b, c)
// -----------------------------------------------------

#define ATLASNET_FOR_EACH(M, SEP, ...) \
  ATLASNET_EXPAND(ATLASNET_CAT(ATLASNET_FE_, ATLASNET_NARGS(__VA_ARGS__))(M, SEP, __VA_ARGS__))

#define ATLASNET_FE_1(M, SEP, a1) M(a1)

#define ATLASNET_FE_2(M, SEP, a1, a2) \
  M(a1) SEP() M(a2)

#define ATLASNET_FE_3(M, SEP, a1, a2, a3) \
  M(a1) SEP() M(a2) SEP() M(a3)

#define ATLASNET_FE_4(M, SEP, a1, a2, a3, a4) \
  M(a1) SEP() M(a2) SEP() M(a3) SEP() M(a4)

#define ATLASNET_FE_5(M, SEP, a1, a2, a3, a4, a5) \
  M(a1) SEP() M(a2) SEP() M(a3) SEP() M(a4) SEP() M(a5)

#define ATLASNET_FE_6(M, SEP, a1, a2, a3, a4, a5, a6) \
  M(a1) SEP() M(a2) SEP() M(a3) SEP() M(a4) SEP() M(a5) SEP() M(a6)

#define ATLASNET_FE_7(M, SEP, a1, a2, a3, a4, a5, a6, a7) \
  M(a1) SEP() M(a2) SEP() M(a3) SEP() M(a4) SEP() M(a5) SEP() M(a6) SEP() M(a7)

#define ATLASNET_FE_8(M, SEP, a1, a2, a3, a4, a5, a6, a7, a8) \
  M(a1) SEP() M(a2) SEP() M(a3) SEP() M(a4) SEP() M(a5) SEP() M(a6) SEP() M(a7) SEP() M(a8)

// -----------------------------------------------------
// Separators
// -----------------------------------------------------
#define ATLASNET_SEP_COMMA() ,
#define ATLASNET_SEP_NONE()


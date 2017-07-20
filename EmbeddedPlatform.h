#pragma once

#include <math.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>

// ----------------------------------------------------------------------------

#ifndef EP_PROFILE
#define EP_PROFILE 1
#endif

#ifndef EP_DEBUG
#define EP_DEBUG 1
#endif

#ifndef EP_LOGGING
#define EP_LOGGING 1
#endif

// ----------------------------------------------------------------------------
// C++98 polyfill.

#ifdef A_CPP_98_COMPILER
#define EP_BUILD_SOME_EMBEDDED_COMPILER

#define static_assert(x,...) typedef int EP_CONCATENATE(EpStaticAssertFail_,__LINE__) [(x) ? 1 : -1]

#define constexpr
#define override
#define final
#define nullptr NULL
#define EP_NOEXCEPT
#define throw(...)
#define assert EpAssert
#define EP_RESTRICT restrict
#define EP_FORCEINLINE
#define EP_LINK_SCRATCHPAD __attribute__(TODO)

#else
#define EP_BUILD_SOFTWARE

#if defined(_MSC_FULL_VER) && _MSC_FULL_VER < 190023026
#define EP_NOEXCEPT
#else
#define EP_NOEXCEPT noexcept
#endif

#define EP_RESTRICT __restrict
#define EP_FORCEINLINE // __forceinline conflicts with inline.
#define EP_LINK_SCRATCHPAD

#endif // !EP_BUILD_SOME_EMBEDDED_COMPILER

// ----------------------------------------------------------------------------

#define EP_QUOTE_(x) #x
#define EP_QUOTE(x) EP_QUOTE_(x)
#define EP_CONCATENATE_(x, y) x ## y
#define EP_CONCATENATE(x, y) EP_CONCATENATE_(x, y)

// ----------------------------------------------------------------------------
// Exclude C++ API when used in C.  This allows stdafx.h to be usable from C.
#if defined(__cplusplus)

template<class T> inline T EpAbs(T x) { return (x >= (T)0) ? x : ((T)0 - x); }
template<class T> inline T EpMax(T x, T y) { return (x > y) ? x : y; }
template<class T> inline T EpMin(T x, T y) { return (x < y) ? x : y; }
template<class T> inline T EpClamp(T x, T min_, T max_) { return (x <= min_) ? min_ : ((x >= max_) ? max_ : x); }

// Use a union to cast pointers.  Helps with compilers that observe the C rules for that.
template<class T, class U> T& EpAliasingCast(U& u) { union { T* t; U* u; } x; x.u = &u; return *x.t; }
template<class T, class U> T* EpAliasingCast(U* u) { union { T* t; U* u; } x; x.u = u; return  x.t; }

enum EpLogLevel {
  EpLogLevel_Log, // No automatic newline
  EpLogLevel_Warning,
  EpLogLevel_Assert
};

// Human readable message required for release asserts.
#define EpReleaseAssertMsg(x, ...) (void)(!!(x) || (EpLogHandler(EpLogLevel_Assert, __VA_ARGS__), (EpAssertHandler(__FILE__, __LINE__), 0)))
#define EpReleaseWarning(x, ...) (void)(!!(x) || (EpLogHandler(EpLogLevel_Warning, __VA_ARGS__), 0))

#define EpIsDebug() EP_DEBUG

#if (EP_DEBUG==1)
#define EpAssertMsg(x, ...) (void)(!!(x) || (EpLogHandler(EpLogLevel_Assert, __VA_ARGS__), EpAssertHandler(__FILE__, __LINE__), 0))
#define EpAssert(x) (void)((!!(x)) || (EpLogHandler(EpLogLevel_Assert, #x), (EpAssertHandler(__FILE__, __LINE__), 0)))
#define EpThrow(x) (void)(EpLogHandler(EpLogLevel_Assert, x), (EpAssertHandler(__FILE__, __LINE__), 0))
#define EpValidate(x) (void)(x).Validate()
#define EpDebugWarning(x, ...) (void)(!!(x) || (EpLogHandler(EpLogLevel_Warning, __VA_ARGS__), 0))
#define EpInit() (void)(s_epIsInit || (EpInitAt(__FILE__, __LINE__), 0))
#else
#define EpAssertMsg(x, ...) ((void)0)
#define EpAssert(x) ((void)0)
#define EpThrow(x) ((void)0)
#define EpValidate(x) ((void)0)
#define EpDebugWarning(x, ...) ((void)0)
#define EpInit() (void)(s_epIsInit || (EpInitAt(0, 0), 0))
#endif

#if (EP_LOGGING==1)
#define EpLog(...) EpLogHandler(EpLogLevel_Log, __VA_ARGS__)
#else
#define EpLog(...) ((void)0)
#endif

#define EP_ALIGNMENT ((uintptr_t)0x4)
#define EP_ALIGNMENT_MASK (EP_ALIGNMENT-(uintptr_t)1)
#define EpIsAligned(x) (((uintptr_t)(x) & (uintptr_t)EP_ALIGNMENT_MASK) == (uintptr_t)0)
#define EpAssertAligned(x) EpAssert(EpIsAligned(x))

extern bool s_epIsInit;
extern float g_epNAN;

void EpInitAt(const char* file=0, unsigned line=0);
void EpShutdown(); // Expects all non-debug allocations to be released.
void EpExit(const char* msg = 0);
void EpAssertHandler(const char* file, unsigned line);
void EpLogHandler(EpLogLevel level, const char* format, ...);
void EpLogHandlerV(EpLogLevel level, const char* format, va_list args);
void EpLogStatus();
void* EpMalloc(size_t size);
void* EpMallocExtended(size_t size, uintptr_t alignmentMask, int memoryAllocatorId=-1); // -1 is EpMemoryAllocatorId_UNSPECIFIED
void EpFree(void *ptr);
void EpHexDump(const void *p, unsigned bytes, const char* label);
void EpFloatDump(const float *ptr, unsigned count, const char* label);
bool EpIsFinite(float f);
bool EpIsScratchpad(void * ptr);
const char* EpBasename(const char* path);

template <class T> T* EpNew(int memoryAllocatorId) { void* buf = EpMallocExtended(sizeof(T), EP_ALIGNMENT_MASK, memoryAllocatorId); return new(buf)T; }
template <class T> void EpDelete(T* t) { if (t) { t->~T(); EpFree(t); } }


#endif // defined(__cplusplus)


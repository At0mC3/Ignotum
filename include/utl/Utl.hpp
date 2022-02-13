#ifndef INCLUDE_UTL_
#define INCLUDE_UTL_

#if defined(_MSC_VER)
    #define FORCE_INLINE  __forceinline inline
    #define HOT_PATH 
#elif defined(__GNUC__) || defined(__clang__)
    #define FORCE_INLINE __attribute__((always_inline)) inline
    #define HOT_PATH __attribute__((flatten))
#else
    #define FORCE_INLINE inline
    #define HOT_PATH 
#endif


namespace Utl
{

}

#endif
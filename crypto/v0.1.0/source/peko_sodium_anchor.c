/*
 * peko_sodium_anchor.c
 * Forces libsodium to be fully linked regardless of link order.
 * Each platform needs its own way to stop the linker from dropping
 * sodium symbols before the references to them are resolved.
 */

#define SODIUM_STATIC 1
#include "libsodium/sodium.h"

#if defined(_MSC_VER)
/* MSVC and lld-link: force the sodium_init symbol to be pulled from the .lib */
#  pragma comment(linker, "/include:sodium_init")
#elif defined(__GNUC__) || defined(__clang__)
/* GCC and Clang: a volatile function pointer stops dead-stripping */
__attribute__((used))
static int (*volatile _sodium_anchor)(void) = sodium_init;
#endif

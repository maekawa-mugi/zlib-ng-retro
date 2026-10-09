/* Host-only macro routing regression: no EE assembly required. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef struct deflate_state deflate_state;
#include "../../arch/mips/mips_functions.h"
#define STRINGIFY_IMPL(x) #x
#define STRINGIFY(x) STRINGIFY_IMPL(x)
#if !defined(MIPS_MMI_NATIVE)
#  error MMI native feature must be active for this test
#endif
#if !defined(native_slide_hash) || !defined(native_chunkmemset_safe)
#  error MMI slide hash and mixed LZ77 copy must be available
#endif
#if defined(TEST_NO_DISPATCH)
#  if defined(native_compare256) || defined(native_longest_match) || \
      defined(native_adler32) || defined(native_adler32_copy) || \
      defined(native_crc32) || defined(native_crc32_copy)
#    error MMI checksums or compare cannot be production defaults
#  endif
#elif defined(TEST_ALL_DISPATCH)
#  if !defined(native_compare256) || !defined(native_longest_match) || \
      !defined(native_adler32) || !defined(native_adler32_copy) || \
      !defined(native_crc32) || !defined(native_crc32_copy)
#    error Explicit native dispatch must enable all selected functions
#  endif
#else
#  error Select TEST_NO_DISPATCH or TEST_ALL_DISPATCH
#endif
int main(void) {
    int ok = strcmp(STRINGIFY(native_chunkmemset_safe), "chunkmemset_safe_mmi") == 0;
#if defined(TEST_ALL_DISPATCH)
    ok &= strcmp(STRINGIFY(native_compare256), "compare256_mmi") == 0;
    ok &= strcmp(STRINGIFY(native_adler32), "adler32_mmi") == 0;
    ok &= strcmp(STRINGIFY(native_crc32_copy), "crc32_copy_chorba_mmi") == 0;
#endif
    return !ok;
}

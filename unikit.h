#ifndef UNIKIT_H_INCLUDED
#define UNIKIT_H_INCLUDED

/*
 * unikit.h
 * ========
 * 
 * Main header for the Unikit library.
 * 
 * Requirements
 * ------------
 * 
 * The unikit_data.c module is the only requirement.  It stores the data
 * tables needed by this library.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Constants
 * =========
 */

/*
 * The Unicode General Category constants.
 * 
 * Every Unicode codepoint is assigned exactly one of the following 30
 * general categories.
 * 
 * General categories always have a two-letter code that contains an
 * uppercase letter A-Z followed by a lowercase letter a-z.  Unikit
 * encodes general categories in unsigned 16-bit integers, where the
 * most significant 8 bits store the ASCII code of the uppercase letter
 * and the least significant 8 bits store the ASCII code of the
 * lowercase letter.  The constants defined below contain the unsigned
 * 16-bit integer form of every Unicode general category.
 * 
 * General categories can further be sorted into category groups.  See
 * the CTGR constants for further information.
 */
#define UNIKIT_GCAT_Lu UINT16_C(0x4c75) /* Lu: Uppercase letter */
#define UNIKIT_GCAT_Ll UINT16_C(0x4c6c) /* Ll: Lowercase letter */
#define UNIKIT_GCAT_Lt UINT16_C(0x4c74) /* Lt: Titlecase digraph */
#define UNIKIT_GCAT_Lm UINT16_C(0x4c6d) /* Lm: Modifier letter */
#define UNIKIT_GCAT_Lo UINT16_C(0x4c6f) /* Lo: Other letter */

#define UNIKIT_GCAT_Mn UINT16_C(0x4d6e) /* Mn: Nonspacing mark */
#define UNIKIT_GCAT_Mc UINT16_C(0x4d63) /* Mc: Spacing mark */
#define UNIKIT_GCAT_Me UINT16_C(0x4d65) /* Me: Enclosing mark */

#define UNIKIT_GCAT_Nd UINT16_C(0x4e64) /* Nd: Decimal digit */
#define UNIKIT_GCAT_Nl UINT16_C(0x4e6c) /* Nl: Letter-like numeric */
#define UNIKIT_GCAT_No UINT16_C(0x4e6f) /* No: Other numeric */

#define UNIKIT_GCAT_Pc UINT16_C(0x5063) /* Pc: Connector punctuation */
#define UNIKIT_GCAT_Pd UINT16_C(0x5064) /* Pd: Dash punctuation */
#define UNIKIT_GCAT_Ps UINT16_C(0x5073) /* Ps: Opening punctuation */
#define UNIKIT_GCAT_Pe UINT16_C(0x5065) /* Pe: Closing punctuation */
#define UNIKIT_GCAT_Pi UINT16_C(0x5069) /* Pi: Initial quotation mark */
#define UNIKIT_GCAT_Pf UINT16_C(0x5066) /* Pf: Final quotation mark */
#define UNIKIT_GCAT_Po UINT16_C(0x506f) /* Po: Other punctuation */

#define UNIKIT_GCAT_Sm UINT16_C(0x536d) /* Sm: Math symbol */
#define UNIKIT_GCAT_Sc UINT16_C(0x5363) /* Sc: Currency symbol */
#define UNIKIT_GCAT_Sk UINT16_C(0x536b) /* Sk: Modifier symbol */
#define UNIKIT_GCAT_So UINT16_C(0x536f) /* So: Other symbol */

#define UNIKIT_GCAT_Zs UINT16_C(0x5a73) /* Zs: Space character */
#define UNIKIT_GCAT_Zl UINT16_C(0x5a6c) /* Zl: only for U+2028 LS */
#define UNIKIT_GCAT_Zp UINT16_C(0x5a70) /* Zp: only for U+2029 PS */

#define UNIKIT_GCAT_Cc UINT16_C(0x4363) /* Cc: C0/C1 control code */
#define UNIKIT_GCAT_Cf UINT16_C(0x4366) /* Cf: format control code */
#define UNIKIT_GCAT_Cs UINT16_C(0x4373) /* Cs: surrogate codepoint */
#define UNIKIT_GCAT_Co UINT16_C(0x436f) /* Co: private use codepoint */
#define UNIKIT_GCAT_Cn UINT16_C(0x436e) /* Cn: reserved or unassigned */

/*
 * The Unicode category group constants.
 * 
 * Category groups divide the 30 general categories covered by the
 * UNIKIT_GCAT constants into 7 groups.
 * 
 * To classify a Unicode category into a category group, mask off all
 * but the 8 most significant bits by performing bitwise AND with the
 * UNIKIT_CTGR_MASK constant.  Then, compare the masked result with the
 * desired category group constant.
 * 
 * The special "LC" (cased letter) group is not defined here.  To check
 * for that, you should instead check whether Unicode category is Lu,
 * Ll, or Lt.
 */
#define UNIKIT_CTGR_MASK      UINT16_C(0xff00)

#define UNIKIT_CTGR_L UINT16_C(0x4c00)  /* Group L: Letters */
#define UNIKIT_CTGR_M UINT16_C(0x4d00)  /* Group M: Combining Marks */
#define UNIKIT_CTGR_N UINT16_C(0x4e00)  /* Group N: Numbers */
#define UNIKIT_CTGR_P UINT16_C(0x5000)  /* Group P: Punctuation */
#define UNIKIT_CTGR_S UINT16_C(0x5300)  /* Group S: Symbols */
#define UNIKIT_CTGR_Z UINT16_C(0x5a00)  /* Group Z: Separatos */
#define UNIKIT_CTGR_C UINT16_C(0x4300)  /* Group C: Other */

/*
 * Type declarations
 * =================
 */

/*
 * Represents a case folding query result.
 */
typedef struct {
  /*
   * The codepoint sequence that is the result of case folding.
   */
  int32_t cpa[4];
  
  /*
   * The length of the cpa array, in range 1 to 4.
   */
  uint8_t len;
  
} UNIKIT_FOLD;

/*
 * Function pointer types
 * ======================
 */

/*
 * Callback for handling errors that occur within the Unikit module.
 * 
 * This callback function must not return.  Undefined behavior occurs if
 * the function returns.  The function is expected to stop the program
 * on an error.
 * 
 * lnum is the line number in the unikit.c source code that the error
 * was raised.  It is always present.
 * 
 * pDetail is the error message, or NULL if there is no specific error
 * message.  The error message does not include any line break.
 * 
 * Parameters:
 * 
 *   lnum - the Unikit source code line number
 * 
 *   pDetail - the error message, or NULL
 */
typedef void (*unikit_fp_err)(int lnum, const char *pDetail);

/*
 * Public functions
 * ================
 */

/*
 * Initialize the Unikit module.
 * 
 * This function must be called before any other function in this
 * module.  You may optionally specify a custom error handler, or set it
 * to NULL for a default handler.  See the unikit_fp_err documentation
 * for further information about the handler.
 * 
 * Parameters:
 * 
 *   fpErr - the custom error handler, or NULL for a default handler
 */
void unikit_init(unikit_fp_err fpErr);

/*
 * Check whether the given integer value is a valid Unicode codepoint.
 * 
 * Valid Unicode codepoints are in range U+0000 to U+10FFFF, excluding
 * the surrogate range U+D800 to U+DFFF.
 * 
 * Parameters:
 * 
 *   cv - the integer value to check
 * 
 * Return:
 * 
 *   non-zero if a valid Unicode codepoint, zero if not
 */
int unikit_valid(int32_t cv);

/*
 * Perform case folding on a given codepoint.
 * 
 * cv is the codepoint to case fold.  It must pass unikit_valid().
 * 
 * pf is the structure that will be filled in with the results of the
 * case folding.  Case folding maps each input codepoint to a sequence
 * of one to four case-folded codepoints.
 * 
 * Most codepoints have a "trivial" case folding, which means their case
 * folding is just a single-codepoint string consisting of the codepoint
 * itself.  This function returns a proper UNIKIT_FOLD structure even in
 * the case of a trivial mapping.  The return value can be used to
 * determine whether or not the mapping is trivial.
 * 
 * Parameters:
 * 
 *   pf - the structure to fild with the case folding
 * 
 *   cv - the Unicode codepoint to query
 * 
 * Return:
 * 
 *   non-zero if non-trivial case folding, zero if trivial case folding
 */
int unikit_fold(UNIKIT_FOLD *pf, int32_t cv);

/*
 * Given any integer value, return the Unicode General Category of the
 * corresponding codepoint.
 * 
 * This function works on all possible integer values, including
 * negative values.  cv does NOT need to pass unikit_valid().
 * Out-of-range integer values will return UNIKIT_GCAT_Cn.
 * 
 * The return value encodes the Unicode General Category with the ASCII
 * code of the uppercase letter in the most significant 8 bits and the
 * ASCII code of the lowercase letter in the least significant 8 bits.
 * The UNIKIT_GCAT constants are the predefined values of the recognized
 * Unicode General Categories.  See the documentation of those constants
 * for further information.
 * 
 * You can also transform the returned general category into a category
 * group by performing a bitwise AND with UNIKIT_CTGR_MASK.  See the
 * UNIKIT_CTGR constants for further information.
 * 
 * Parmeters:
 * 
 *   cv - the integer codepoint value
 * 
 * Return:
 * 
 *   the Unicode General Category ASCII letters encoded in a 16-bit
 *   integer
 */
uint16_t unikit_category(int32_t cv);

#endif

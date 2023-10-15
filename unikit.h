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

#endif

/*
 * unikit_query.c
 * ==============
 * 
 * Perform queries using Unikit's embedded data tables.
 * 
 * Syntax
 * ------
 * 
 *   unikit_query fold U+004D
 * 
 * Unicode codepoint parameters start with "U+" (case insensitive) and
 * are followed by 1 to 6 base-16 digits (case insensitive).
 * 
 * Description
 * -----------
 * 
 * The "fold" query returns the case folding of the given codepoint,
 * which is a sequence of one to four codepoints.  Most codepoints have
 * trivial foldings, where the case folding is just the given codepoint.
 * 
 * Requirements
 * ------------
 * 
 * Requires the unikit library, which consists of the unikit.c and
 * unikit_data.c modules.
 * 
 * Requires the diagnostic.c module, which is contained within this test
 * directory.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "unikit.h"

/*
 * Diagnostics
 * ===========
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}
 
static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void unikit_err(int lnum, const char *pDetail);
static int32_t parseCodepoint(const char *pstr);

/*
 * Custom error handler for Unikit library.
 */
static void unikit_err(int lnum, const char *pDetail) {
  if (pDetail != NULL) {
    raiseErr(__LINE__, "[Unikit error, line %d] %s",
              lnum, pDetail);
  } else {
    raiseErr(__LINE__, "[Unikit error, line %d] Error", lnum);
  }
}

/*
 * Parse a string containing a codepoint in "U+004D" format.
 * 
 * No extra whitespace is allowed.  However, the string is case
 * insensitive and can have anywhere from 1 to 6 base-16 digits.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 * Return:
 * 
 *   the parsed codepoint value, in range 0x0000 to 0x10ffff
 */
static int32_t parseCodepoint(const char *pstr) {
  
  size_t slen = 0;
  int32_t result = 0;
  int c = 0;
  
  /* Check parameters */
  if (pstr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check for opening "U+" prefix */
  if (((pstr[0] != 'U') && (pstr[0] != 'u')) || (pstr[1] != '+')) {
    raiseErr(__LINE__, "Invalid codepoint parameter");
  }
  
  /* Skip prefix */
  pstr += 2;
  
  /* Get remaining string length and make sure in range 1-6 */
  slen = strlen(pstr);
  if ((slen < 1) || (slen > 6)) {
    raiseErr(__LINE__, "Invalid codepoint parameter");
  }
  
  /* Parse digits */
  result = 0;
  for( ; *pstr != 0; pstr++) {
    c = *pstr;
    if ((c >= '0') && (c <= '9')) {
      c = c - '0';
    } else if ((c >= 'A') && (c <= 'F')) {
      c = (c - 'A') + 10;
    } else if ((c >= 'a') && (c <= 'f')) {
      c = (c - 'a') + 10;
    } else {
      raiseErr(__LINE__, "Invalid codepoint parameter");
    }
    
    result = (result << 4) | ((int32_t) c);
  }
  
  /* Check range */
  if ((result < 0) || (result > 0x10ffff)) {
    raiseErr(__LINE__, "Codepoint parameter out of range");
  }
  
  /* Return result */
  return result;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int32_t cv = 0;
  int i = 0;
  UNIKIT_FOLD fold;
  
  /* Initialize structures */
  memset(&fold, 0, sizeof(UNIKIT_FOLD));
  
  /* Initialize diagnostics and check parameters */
  diagnostic_startup(argc, argv, "unikit_query");
  
  /* Initialize unikit */
  unikit_init(&unikit_err);
  
  /* Parse specific subprogram invocation */
  if (argc < 2) {
    raiseErr(__LINE__, "Expecting program arguments");
  }
  if (strcmp(argv[1], "fold") == 0) {
    /* Case folding query -- must have one argument beyond mode */
    if (argc != 3) {
      raiseErr(__LINE__, "Wrong number of arguments for fold");
    }
    
    /* Parse argument and perform query */
    cv = parseCodepoint(argv[2]);
    if (!unikit_valid(cv)) {
      raiseErr(__LINE__, "Codepoint out of range");
    }
    unikit_fold(&fold, cv);
    
    /* Print the case folding */
    for(i = 0; i < fold.len; i++) {
      if (i > 0) {
        printf(" ");
      }
      printf("U+%04lx", (long) (fold.cpa)[i]);
    }
    printf("\n");
    
  } else {
    raiseErr(__LINE__, "Unrecognized subprogram: %s", argv[1]);
  }
  
  /* Return successfully */
  return EXIT_SUCCESS;
}

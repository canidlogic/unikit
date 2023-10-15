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
 *   unikit_query gencat U+004D
 *   unikit_query gentab
 *   unikit_query genrange Sm
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
 * The "gencat" query returns the Unicode General Category of the
 * requested codepoint.  You can pass surrogate codepoints to classify
 * with this invocation as well.
 * 
 * The "gentab" query goes through all codepoints from U+0000 to
 * U+10FFFF and tabulates how many total codepoints are in each
 * category.
 * 
 * The "genrange" query requires a Unicode General Category as a
 * parameter.  It goes through all codepoints from U+0000 to U+10FFFF
 * and prints out all codepoint ranges that have the given general
 * category.
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
 * Type declarations
 * =================
 */

/*
 * "gentab" subprogram record.
 */
typedef struct {
  
  /*
   * The category code this record is tabulating.
   */
  uint16_t gencat;
  
  /*
   * The order this record appears on output.
   */
  int rec_ord;
  
  /*
   * The total number of codepoints for this record.
   */
  int32_t count;
  
} GTAB;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void unikit_err(int lnum, const char *pDetail);
static int32_t parseCodepoint(const char *pstr);

static int gentab_cat_cmp(const void *pA, const void *pB);
static int gentab_ord_cmp(const void *pA, const void *pB);
static int gentab_search_cmp(const void *pKey, const void *pEl);
static void gentab(void);

static void genrange(uint16_t catcode);

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
 * This function DOES allow surrogate codepoints through.
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
 * qsort comparison function that sorts GTAB records in ascending order
 * of their gencat field.
 */
static int gentab_cat_cmp(const void *pA, const void *pB) {
  
  const GTAB *rA = NULL;
  const GTAB *rB = NULL;
  int result = 0;
  
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  rA = (const GTAB *) pA;
  rB = (const GTAB *) pB;
  
  if (rA->gencat < rB->gencat) {
    result = -1;
    
  } else if (rA->gencat > rB->gencat) {
    result = 1;
    
  } else if (rA->gencat == rB->gencat) {
    result = 0;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  return result;
}

/*
 * qsort comparison function that sorts GTAB records in ascending order
 * of their rec_ord field.
 */
static int gentab_ord_cmp(const void *pA, const void *pB) {
  
  const GTAB *rA = NULL;
  const GTAB *rB = NULL;
  int result = 0;
  
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  rA = (const GTAB *) pA;
  rB = (const GTAB *) pB;
  
  if (rA->rec_ord < rB->rec_ord) {
    result = -1;
    
  } else if (rA->rec_ord > rB->rec_ord) {
    result = 1;
    
  } else if (rA->rec_ord == rB->rec_ord) {
    result = 0;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  return result;
}

/*
 * bsearch key comparison function that has a uint16_t key that is
 * compared against the gencat field of a GTAB record.
 */
static int gentab_search_cmp(const void *pKey, const void *pEl) {
  
  const uint16_t *rKey = NULL;
  const GTAB     *rEl  = NULL;
  int result = 0;
  
  if ((pKey == NULL) || (pEl == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  rKey = (const uint16_t *) pKey;
  rEl  = (const GTAB     *) pEl;
  
  if (*rKey < rEl->gencat) {
    result = -1;
    
  } else if (*rKey > rEl->gencat) {
    result = 1;
    
  } else if (*rKey == rEl->gencat) {
    result = 0;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  return result;
}

/*
 * The "gentab" subprogram.
 */
static void gentab(void) {
  
  GTAB rec[30];
  GTAB *pr = NULL;
  int32_t i = 0;
  uint16_t ccat = 0;
  
  uint16_t buf_cat = 0;
  int32_t buf_count = 0;
  
  /* Initialize records */
  memset(rec, 0, sizeof(GTAB) * 30);
  
  for(i = 0; i < 30; i++) {
    rec[i].rec_ord = (int) i;
    rec[i].count   = 0;
  }
  
  rec[ 0].gencat = UNIKIT_GCAT_Lu;
  rec[ 1].gencat = UNIKIT_GCAT_Ll;
  rec[ 2].gencat = UNIKIT_GCAT_Lt;
  rec[ 3].gencat = UNIKIT_GCAT_Lm;
  rec[ 4].gencat = UNIKIT_GCAT_Lo;

  rec[ 5].gencat = UNIKIT_GCAT_Mn;
  rec[ 6].gencat = UNIKIT_GCAT_Mc;
  rec[ 7].gencat = UNIKIT_GCAT_Me;

  rec[ 8].gencat = UNIKIT_GCAT_Nd;
  rec[ 9].gencat = UNIKIT_GCAT_Nl;
  rec[10].gencat = UNIKIT_GCAT_No;

  rec[11].gencat = UNIKIT_GCAT_Pc;
  rec[12].gencat = UNIKIT_GCAT_Pd;
  rec[13].gencat = UNIKIT_GCAT_Ps;
  rec[14].gencat = UNIKIT_GCAT_Pe;
  rec[15].gencat = UNIKIT_GCAT_Pi;
  rec[16].gencat = UNIKIT_GCAT_Pf;
  rec[17].gencat = UNIKIT_GCAT_Po;

  rec[18].gencat = UNIKIT_GCAT_Sm;
  rec[19].gencat = UNIKIT_GCAT_Sc;
  rec[20].gencat = UNIKIT_GCAT_Sk;
  rec[21].gencat = UNIKIT_GCAT_So;

  rec[22].gencat = UNIKIT_GCAT_Zs;
  rec[23].gencat = UNIKIT_GCAT_Zl;
  rec[24].gencat = UNIKIT_GCAT_Zp;

  rec[25].gencat = UNIKIT_GCAT_Cc;
  rec[26].gencat = UNIKIT_GCAT_Cf;
  rec[27].gencat = UNIKIT_GCAT_Cs;
  rec[28].gencat = UNIKIT_GCAT_Co;
  rec[29].gencat = UNIKIT_GCAT_Cn;
  
  /* Sort records according to general category */
  qsort(rec, (size_t) 30, sizeof(GTAB), &gentab_cat_cmp);
  
  /* Tabulate codepoints */
  for(i = 0; i <= 0x10ffff; i++) {
    /* Get category */
    ccat = unikit_category(i);
    
    /* Flush buffer if not relevant */
    if ((buf_count > 0) && (buf_cat != ccat)) {
      pr = (GTAB *) bsearch(
                      &buf_cat, rec,
                      30, sizeof(GTAB),
                      &gentab_search_cmp);
      if (pr == NULL) {
        raiseErr(__LINE__, "Unrecognized category encountered",
          buf_cat);
      }
      pr->count += buf_count;
      buf_count = 0;
    }
    
    /* Update buffer */
    buf_cat = ccat;
    buf_count++;
  }
  
  /* Flush buffer */
  if (buf_count > 0) {
    pr = (GTAB *) bsearch(
                    &buf_cat, rec,
                    30, sizeof(GTAB),
                    &gentab_search_cmp);
    if (pr == NULL) {
      raiseErr(__LINE__, "Unrecognized category encountered");
    }
    pr->count += buf_count;
    buf_count = 0;
  }
  
  /* Sort records according to display order */
  qsort(rec, (size_t) 30, sizeof(GTAB), &gentab_ord_cmp);
  
  /* Display results */
  for(i = 0; i < 30; i++) {
    putchar((int) (rec[i].gencat >> 8));
    putchar((int) (rec[i].gencat & 0xff));
    printf(" : %6ld\n", (long) rec[i].count);
  }
}

/*
 * The "genrange" subprogram.
 */
static void genrange(uint16_t catcode) {
  
  int32_t i = 0;
  
  int buf_fill = 0;
  int32_t buf_low = 0;
  int32_t buf_hi  = 0;
  
  char gcat[3];
  
  /* Initialize buffer */
  memset(gcat, 0, 3);
  
  /* Check parameter */
  if (((catcode & 0xff00) == 0) || ((catcode & 0x00ff) == 0) ||
      ((catcode & 0x8080) != 0)) {
    raiseErr(__LINE__, "Invalid category code");
  }
  
  /* Get string form for category */
  gcat[0] = (char) (catcode >> 8);
  gcat[1] = (char) (catcode & 0xff);
  
  /* Go through all codepoints */
  for(i = 0; i <= 0x10ffff; i++) {
    /* Check if this codepoint is part of requested category */
    if (unikit_category(i) == catcode) {
      /* Part of requested category, so start buffer if not filled */
      if (!buf_fill) {
        buf_fill = 1;
        buf_low = i;
        buf_hi = i;
      }
      
      /* Update buffer */
      buf_hi = i;
      
    } else {
      /* Not part of requested category, so flush buffer if filled */
      if (buf_fill) {
        printf("%04lx - %04lx [%s]\n",
          (long) buf_low,
          (long) buf_hi,
          gcat);
        buf_fill = 0;
      }
    }
  }
  
  /* Flush buffer if filled */
  if (buf_fill) {
    printf("%04lx - %04lx [%s]\n",
      (long) buf_low,
      (long) buf_hi,
      gcat);
    buf_fill = 0;
  }
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int32_t cv = 0;
  int i = 0;
  uint16_t retval = 0;
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
  
  } else if (strcmp(argv[1], "gencat") == 0) {
    /* General category query -- must have one argument beyond mode */
    if (argc != 3) {
      raiseErr(__LINE__, "Wrong number of arguments for gencat");
    }
    
    /* Parse argument and perform query */
    cv = parseCodepoint(argv[2]);
    retval = unikit_category(cv);
    
    /* Print the result */
    printf("U+%04lx : ", (long) cv);
    putchar((int) (retval >> 8));
    putchar((int) (retval & 0xff));
    printf("\n");
  
  } else if (strcmp(argv[1], "gentab") == 0) {
    /* General category tabulation -- no extra arguments */
    if (argc != 2) {
      raiseErr(__LINE__, "Wrong number of arguments for gentab");
    }
    
    /* Invoke subprogram */
    gentab();
  
  } else if (strcmp(argv[1], "genrange") == 0) {
    /* Range for a general category -- must have one argument beyond
     * mode */
    if (argc != 3) {
      raiseErr(__LINE__, "Wrong number of arguments for gencat");
    }
    
    /* Argument must have length exactly two with first letter A-Z and
     * second letter a-z */
    if (strlen(argv[2]) != 2) {
      raiseErr(__LINE__, "Invalid category: %s", argv[2]);
    }
    if ((argv[2][0] < 'A') || (argv[2][0] > 'Z') ||
        (argv[2][1] < 'a') || (argv[2][1] > 'z')) {
      raiseErr(__LINE__, "Invalid category: %s", argv[2]);
    }
    
    /* Invoke subprogram */
    genrange((uint16_t) (((uint16_t) argv[2][0]) << 8) |
                         ((uint16_t) argv[2][1]));
    
  } else {
    raiseErr(__LINE__, "Unrecognized subprogram: %s", argv[1]);
  }
  
  /* Return successfully */
  return EXIT_SUCCESS;
}

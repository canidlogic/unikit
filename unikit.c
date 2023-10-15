/*
 * unikit.c
 * ========
 * 
 * Implementation of unikit.h
 * 
 * See the header for further information.
 */

#include "unikit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unikit_data.h"

/*
 * Diagnostics
 * ===========
 */

/*
 * Callback function used for errors, or NULL to use the default
 * implementation.
 */
static unikit_fp_err m_err = NULL;

/*
 * Raise an error.
 * 
 * This function does not return.  It will use a client-provided
 * callback m_err if available, or otherwise a default implementation.
 * 
 * Parameters:
 * 
 *   lnum - the line number in this source file
 * 
 *   pDetail - the error message, or NULL
 */
static void raiseErr(int lnum, const char *pDetail) {
  if (m_err != NULL) {
    m_err(lnum, pDetail);
    
  } else {
    fprintf(stderr, "[Unikit error, line %d] ", lnum);
    if (pDetail != NULL) {
      fprintf(stderr, "%s\n", pDetail);
    } else {
      fprintf(stderr, "Generic error\n");
    }
    exit(EXIT_FAILURE);
  }
}

/*
 * Local data
 * ==========
 */

/*
 * Flag set to non-zero once the module is initialized.
 */
static int m_init = 0;

/*
 * The case folding indices and data table, along with their lengths.
 */
static uint16_t *m_case_lower = NULL;
static uint16_t *m_case_upper = NULL;
static uint16_t *m_case_data  = NULL;

static int32_t m_case_lower_len = 0;
static int32_t m_case_upper_len = 0;
static int32_t m_case_data_len = 0;

/*
 * The general category tables, along with their lengths.
 */
static uint16_t *m_gcat_core = NULL;
static uint16_t *m_gcat_gen_low = NULL;
static uint16_t *m_gcat_gen_high = NULL;
static uint16_t *m_gcat_bitmap = NULL;
static uint16_t *m_gcat_astral = NULL;

static int32_t m_gcat_core_len = 0;
static int32_t m_gcat_gen_low_len = 0;
static int32_t m_gcat_gen_high_len = 0;
static int32_t m_gcat_bitmap_len = 0;
static int32_t m_gcat_astral_len = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static uint16_t *decodeUint16Array(const char *pStr, int32_t *pLen);
static uint16_t queryTrie(
    const uint16_t *pTrie,
          int32_t   tlen,
          uint32_t  key,
          int       depth);

/*
 * Decode a nul-terminated base-64 string with big endian ordering into
 * an array of unsigned 16-bit integers.
 * 
 * The returned array is dynamically allocated.
 * 
 * The base-64 string length must be greater than zero and a multiple of
 * four.  Padding = signs must be used if necessary in the last group of
 * four base-64 digits.  After decoding, the total byte length must be a
 * multiple of two so that it can be assembled into unsigned 16-bit
 * integers.
 * 
 * The pLen variable will receive the length -- in integers, not
 * bytes -- of the returned array.
 * 
 * Parameters:
 * 
 *   pStr - the base-64 string to decode
 * 
 *   pLen - variable to receive number of elements in the decoded array
 * 
 * Return:
 * 
 *   a dynamically-allocated array decoded from the base-64 string
 */
static uint16_t *decodeUint16Array(const char *pStr, int32_t *pLen) {
  
  int32_t slen = 0;
  int32_t i = 0;
  int32_t j = 0;
  int64_t iv64 = 0;
  int32_t groups = 0;
  int32_t extra = 0;
  int32_t base = 0;
  int c = 0;
  
  uint16_t *pBuf = NULL;
  uint16_t *pb = NULL;
  
  /* Check parameters */
  if (pStr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Get length, not including terminating nul, and make sure greater
   * than zero and multiple of four */
  slen = (int32_t) strlen(pStr);
  if ((slen < 1) || ((slen % 4) != 0)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Count the number of eight-character groups */
  groups = (slen / 8);
  
  /* If there is at least one group, and the last group ends with an =
   * sign, then make sure the total length of the string is an exact
   * multiple of eight and then reduce the group count by one since the
   * last group is incomplete */
  if (groups > 0) {
    if (pStr[(groups * 8) - 1] == '=') {
      if ((slen % 8) != 0) {
        raiseErr(__LINE__, NULL);
      }
      groups--;
    }
  }
  
  /* The group count now applies to all full groups of eight characters,
   * which will decode to six bytes (exactly three 16-bit integers);
   * determine how many extra 16-bit integers there are, if any, beyond
   * these encoded full groups */
  base = groups * 8;
  if (pStr[base] == 0) {
    /* Nothing beyond full groups */
    extra = 0;
    
  } else if (pStr[base + 4] == 0) {
    /* Four extra base-64 characters, so must be one extra integer */
    extra = 1;
    
  } else if (pStr[base + 8] == 0) {
    /* Eight extra base-64 characters that are not a full group, so must
     * be two extra integers */
    extra = 2;
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Compute total number of elements */
  *pLen = (int32_t) ((groups * 3) + extra);
  
  /* Allocate a buffer of the correct length */
  pBuf = (uint16_t *) calloc((size_t) *pLen, sizeof(uint16_t));
  if (pBuf == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  /* Start the buffer pointer at the start */
  pb = pBuf;
  
  /* Decode full groups */
  for(i = 0; i < groups; i++) {
    /* Decode the current group into a 64-bit value */
    iv64 = 0;
    for(j = 0; j < 8; j++) {
      c = *pStr;
      pStr++;
      
      if ((c >= 'A') && (c <= 'Z')) {
        c = c - 'A';
      } else if ((c >= 'a') && (c <= 'z')) {
        c = (c - 'a') + 26;
      } else if ((c >= '0') && (c <= '9')) {
        c = (c - '0') + 52;
      } else if (c == '+') {
        c = 62;
      } else if (c == '/') {
        c = 63;
      } else {
        raiseErr(__LINE__, NULL);
      }
      
      iv64 = (iv64 << 6) | ((int64_t) c);
    }
    
    /* Store the decoded integers and advance pointer */
    pb[0] = (uint16_t) (iv64 >> 32);
    pb[1] = (uint16_t) ((iv64 >> 16) & 0xffff);
    pb[2] = (uint16_t) (iv64 & 0xffff);
    
    pb += 3;
  }
  
  /* Decode any remaining partial group */
  iv64 = 0;
  for(i = 0; i < extra * 3; i++) {
    c = *pStr;
    pStr++;
    
    if ((c >= 'A') && (c <= 'Z')) {
      c = c - 'A';
    } else if ((c >= 'a') && (c <= 'z')) {
      c = (c - 'a') + 26;
    } else if ((c >= '0') && (c <= '9')) {
      c = (c - '0') + 52;
    } else if (c == '+') {
      c = 62;
    } else if (c == '/') {
      c = 63;
    } else {
      raiseErr(__LINE__, NULL);
    }
    
    iv64 = (iv64 << 6) | ((int64_t) c);
  }
  
  /* Byte-align decoded partial value */
  if (extra == 1) {
    iv64 >>= 2;
  } else if (extra == 2) {
    iv64 >>= 4;
  }
  
  /* Store any extra decoded integers */
  if (extra == 1) {
    pb[0] = (uint16_t) (iv64 & 0xffff);
    
  } else if (extra == 2) {
    pb[0] = (uint16_t) (iv64 >> 16);
    pb[1] = (uint16_t) (iv64 & 0xffff);
    
  } else {
    if (extra != 0) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  /* Make sure remainder of string is padding */
  for( ; *pStr != 0; pStr++) {
    if (*pStr != '=') {
      raiseErr(__LINE__, NULL);
    }
  }
  
  /* Return the decoded buffer */
  return pBuf;
}

/*
 * Query a compiled trie.
 * 
 * pTrie is a pointer to the compiled trie, and depth is the depth of
 * the tree, which must be in range 1 to 8.  tlen is the length in
 * integers (not bytes!) of the compiled trie, which is used as a
 * safeguard against out-of-bounds memory access.
 * 
 * The compiled tree is a sequence of tables, where each table is
 * exactly 16 unsigned 16-bit integers.  Each table is either a leaf
 * table or a non-leaf table.  Leaf tables directly store key values, or
 * the special value 0xFFFF indicating no value.  Non-leaf tables store
 * indices of other tables, or the special value 0xFFFF indicating no
 * table.
 * 
 * The requested key encodes a sequence of 4-bit nybbles, with more
 * significant nybbles queried before less significant nybbles.  The
 * total number of nybbles in the key is always equal to the depth, and
 * the last nybble is always the least significant 4 bits of the key.
 * Nybbles of the key that are beyond the nybble length suggested by the
 * depth are ignored.
 * 
 * The first nybble is always used to query the first table in the
 * compiled trie.  Nybbles from the key are always used as an index into
 * the 16 records in each table.  For all nybbles but the last, the
 * table is interpreted as a non-leaf table, and the indices to other
 * tables in the trie are followed.  For the last nybble, the table is
 * interpreted as a leaf table, and the records yield the value.
 * 
 * The return value is either an unsigned 16-bit value in range 0 to
 * 0xFFFE, or the special value 0xFFFF indicating that no record is
 * mapped to this key in the trie.
 * 
 * Parameters:
 * 
 *   pTrie - the compiled trie
 * 
 *   tlen - the length in elements of the compiled trie
 * 
 *   key - the encoded key nybble sequence
 * 
 *   depth - the depth of the trie
 * 
 * Return:
 * 
 *   the value mapped to the key, or 0xFFFF if no mapped value
 */
static uint16_t queryTrie(
    const uint16_t *pTrie,
          int32_t   tlen,
          uint32_t  key,
          int       depth) {
  
  int32_t tptr = 0;
  int i = 0;
  int j = 0;
  uint16_t r = 0;
  
  /* Check parameters */
  if (pTrie == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (tlen < 1) {
    raiseErr(__LINE__, NULL);
  }
  if ((depth < 1) || (depth > 8)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Table offset pointer starts out at first table */
  tptr = 0;
  
  /* Process all nybbles but the last to adjust the table pointer, or
   * set table pointer to -1 if we encountered missing records */
  for(i = 0; i <= depth - 2; i++) {
    /* Get the current nybble */
    j = (int) ((key >> ((depth - i - 1) * 4)) & 0x0f);
    
    /* Get the indexed record and handle missing record */
    if (tptr + j >= tlen) {
      raiseErr(__LINE__, "Trie bound error");
    }
    
    r = pTrie[tptr + j];
    if (r == 0xffff) {
      tptr = -1;
      break;
    }
    
    /* Adjust table pointer offset */
    tptr = ((int32_t) r) * 16;
  }
  
  /* If we still have a valid table pointer, get the result record using
   * the least significant nybble; else, result is 0xffff */
  if (tptr >= 0) {
    j = (int) (key & 0x0f);
    
    if (tptr + j >= tlen) {
      raiseErr(__LINE__, "Trie bound error");
    }
    
    r = pTrie[tptr + j];
    
  } else {
    r = UINT16_C(0xffff);
  }
  
  /* Return the result */
  return r;
}

/*
 * Public functions
 * ================
 * 
 * See the header for specifications.
 */

/*
 * unikit_init function.
 */
void unikit_init(unikit_fp_err fpErr) {
  /* Check not already initialized */
  if (m_init) {
    raiseErr(__LINE__, "Unikit already initialized");
  }
  
  /* Store the error handler (which may be NULL) and update init flag */
  m_err = fpErr;
  m_init = 1;
  
  /* Decode tables */
  m_case_lower = decodeUint16Array(
                  unikit_data_fetch(UNIKIT_DATA_KEY_CASE_LOWER),
                  &m_case_lower_len);
  m_case_upper = decodeUint16Array(
                  unikit_data_fetch(UNIKIT_DATA_KEY_CASE_UPPER),
                  &m_case_upper_len);
  m_case_data  = decodeUint16Array(
                  unikit_data_fetch(UNIKIT_DATA_KEY_CASE_DATA),
                  &m_case_data_len);
  
  m_gcat_core     = decodeUint16Array(
                      unikit_data_fetch(UNIKIT_DATA_KEY_GCAT_CORE),
                      &m_gcat_core_len);
  m_gcat_gen_low  = decodeUint16Array(
                      unikit_data_fetch(UNIKIT_DATA_KEY_GCAT_GEN_LOW),
                      &m_gcat_gen_low_len);
  m_gcat_gen_high = decodeUint16Array(
                      unikit_data_fetch(UNIKIT_DATA_KEY_GCAT_GEN_HIGH),
                      &m_gcat_gen_high_len);
  m_gcat_bitmap   = decodeUint16Array(
                      unikit_data_fetch(UNIKIT_DATA_KEY_GCAT_BITMAP),
                      &m_gcat_bitmap_len);
  m_gcat_astral   = decodeUint16Array(
                      unikit_data_fetch(UNIKIT_DATA_KEY_GCAT_ASTRAL),
                      &m_gcat_astral_len);
}

/*
 * unikit_valid function.
 */
int unikit_valid(int32_t cv) {
  int result = 1;
  
  if ((cv >= 0x0) && (cv <= 0x10ffff)) {
    if ((cv >= 0xd800) && (cv <= 0xdfff)) {
      result = 0;
    }
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * unikit_fold function.
 */
int unikit_fold(UNIKIT_FOLD *pf, int32_t cv) {
  
  int result = 0;
  int plane = 0;
  uint16_t r = 0;
  int sqlen = 0;
  int base = 0;
  int i = 0;
  
  /* Check state */
  if (!m_init) {
    raiseErr(__LINE__, "Unikit not initialized");
  }
  
  /* Check parameters */
  if (pf == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (!unikit_valid(cv)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Clear result structure */
  memset(pf, 0, sizeof(UNIKIT_FOLD));
  
  /* Determine the plane, 0 or 1, or -1 if some other plane */
  if ((cv >= 0) && (cv <= 0xffff)) {
    plane = 0;
  
  } else if ((cv >= 0x10000) && (cv <= 0x1ffff)) {
    plane = 1;
  
  } else {
    plane = -1;
  }
  
  /* If in plane 0 or 1, query the appropriate trie using the 16 least
   * significant bits of the codepoint; if in another plane, just set a
   * failed query result */
  if (plane == 0) {
    r = queryTrie(m_case_lower, m_case_lower_len,
                    (uint32_t) (cv & 0xffff), 4);
    
  } else if (plane == 1) {
    r = queryTrie(m_case_upper, m_case_upper_len,
                    (uint32_t) (cv & 0xffff), 4);
    
  } else {
    r = 0xffff;
  }
  
  /* If we found a record, copy codepoints into result structure;
   * otherwise, use a trivial mapping */
  if (r != 0xffff) {
    /* Extract fields from the data key */
    sqlen = ((int) (r & 0x3)) + 1;
    base = (int) (r >> 2);
    
    /* Check that range is within data array bounds */
    if (base > m_case_data_len - ((int32_t) sqlen)) {
      raiseErr(__LINE__, "Data bound error");
    }
    
    /* Copy the codepoint array, increasing codepoint values by 0x10000
     * if they are in the upper plane */
    pf->len = (uint8_t) sqlen;
    for(i = 0; i < sqlen; i++) {
      (pf->cpa)[i] = (int32_t) m_case_data[base + i];
      if (plane == 1) {
        (pf->cpa)[i] += 0x10000;
      }
    }
    
    /* Determine if mapping is trivial */
    if (pf->len == 1) {
      if ((pf->cpa)[0] == cv) {
        result = 0;
      } else {
        result = 1;
      }
      
    } else {
      result = 1;
    }
    
  } else {
    /* Trivial mapping */
    (pf->cpa)[0] = cv;
    pf->len = UINT8_C(1);
    result = 0;
  }
  
  /* Return trivial indicator */
  return result;
}

/*
 * unikit_category function.
 */
uint16_t unikit_category(int32_t cv) {
  
  uint16_t result = 0;
  int32_t lbound = 0;
  int32_t ubound = 0;
  int32_t mid = 0;
  
  int cr = 0;
  int plane = 0;
  int32_t offs = 0;
  uint16_t r_plane = 0;
  uint16_t r_lower = 0;
  uint16_t r_upper = 0;
  
  /* Check state */
  if (!m_init) {
    raiseErr(__LINE__, "Unikit not initialized");
  }
  
  /* Default result if we don't find anything is Cn */
  result = UNIKIT_GCAT_Cn;
  
  /* Handling depends on range */
  if ((cv >= 0) && (cv <= 0xff)) {
    /* In core range, so use the core lookup */
    if (m_gcat_core_len != 256) {
      raiseErr(__LINE__, "Invalid core table length");
    }
    result = m_gcat_core[cv];
    
  } else if ((cv >= 0x100) && (cv <= 0x1ffff)) {
    /* In general range, so first we want to compute the offset and
     * shift value for this codepoint within the character bitmap */
    offs  = cv - 0x100;
    plane = (int) ((offs % 8) * 2);
    offs  = offs / 8;
    
    /* Get the bitmap value for this codepoint */
    if (offs >= m_gcat_bitmap_len) {
      raiseErr(__LINE__, "Bitmap query out of range");
    }
    result = (uint16_t) ((m_gcat_bitmap[offs] >> plane) & 0x3);
    
    /* Decode the bitmap value */
    if (result == 1) {
      result = UNIKIT_GCAT_Lo;
      
    } else if (result == 2) {
      result = UNIKIT_GCAT_Ll;
      
    } else if (result == 3) {
      result = UNIKIT_GCAT_So;
      
    } else if (result == 0) {
      /* Bitmap didn't answer our question, so our next attempt is to
       * query the general character table tries */
      if (cv <= 0xffff) {
        result = queryTrie(m_gcat_gen_low, m_gcat_gen_low_len,
                    (uint32_t) cv, 4);
      } else {
        result = queryTrie(m_gcat_gen_high, m_gcat_gen_high_len,
                    (uint32_t) (cv & 0xffff), 4);
      }
      
      /* If general character table didn't get a result, our last
       * attempt is to use the hardcoded remainder table */
      if (result == 0xffff) {
        /* Reset result to Cn in case hardcoded tables don't work */
        result = UNIKIT_GCAT_Cn;
        
        /* Check hardcoded tables (derived from the "remainder"
         * invocation  of the unikit_db.pl script) */
        if ((cv >= 0xd800) && (cv <= 0xdfff)) {
          result = UNIKIT_GCAT_Cs;
        
        } else if ((cv >= 0xe000) && (cv <= 0xf8ff)) {
          result = UNIKIT_GCAT_Co;
        }
      }
      
    } else {
      raiseErr(__LINE__, NULL);
    }
    
  } else if ((cv >= 0x20000) && (cv <= 0x10ffff)) {
    /* Astral range, so make sure astral table is non-empty and has
     * length divisible by four */
    if ((m_gcat_astral_len < 1) || ((m_gcat_astral_len % 4) != 0)) {
      raiseErr(__LINE__, "Invalid astral table length");
    }
    
    /* Determine plane and offset of codepoint */
    plane = (int) (cv >> 16);
    offs = (cv & 0xffff);
    
    /* Search lower bound is zero and upper bound is maximum astral
     * record index */
    lbound = 0;
    ubound = (m_gcat_astral_len / 4) - 1;
    
    /* Zoom in on the relevant record */
    while (lbound < ubound) {
      /* Midpoint record is halfway between, and at least one above
       * lower bound */
      mid = lbound + ((ubound - lbound) / 2);
      if (mid <= lbound) {
        mid = lbound + 1;
      }
      
      /* Get key fields of midpoint record */
      r_plane = m_gcat_astral[(mid * 4)    ];
      r_lower = m_gcat_astral[(mid * 4) + 1];
      
      /* Compare query plane and offset to midpoint record plane */
      if (plane < r_plane) {
        cr = -1;
        
      } else if (plane > r_plane) {
        cr = 1;
        
      } else if (plane == r_plane) {
        if (offs < r_lower) {
          cr = -1;
          
        } else if (offs > r_lower) {
          cr = 1;
          
        } else if (offs == r_lower) {
          cr = 0;
          
        } else {
          raiseErr(__LINE__, NULL);
        }
        
      } else {
        raiseErr(__LINE__, NULL);
      }
      
      /* Update boundaries depending on result of comparison */
      if (cr < 0) {
        /* Query is less than midpoint record, so new upper bound is one
         * less than midpoint */
        ubound = mid - 1;
        
      } else if (cr > 0) {
        /* Query is greater than midpoint record, so new lower bound is
         * the midpoint */
        lbound = mid;
        
      } else if (cr == 0) {
        /* Query exactly matches lower bound of record, so zoom in */
        lbound = mid;
        ubound = mid;
        
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* lbound is the selected record, so get its key fields */
    r_plane = m_gcat_astral[(lbound * 4)    ];
    r_lower = m_gcat_astral[(lbound * 4) + 1];
    r_upper = m_gcat_astral[(lbound * 4) + 2];
    
    /* If the requested codepoint is covered by this selected record,
     * then query result is the category in the record; else, leave
     * query result at Cn */
    if ((plane == r_plane) && (offs >= r_lower) && (offs <= r_upper)) {
      result = m_gcat_astral[(lbound * 4) + 3];
    }
  }
  
  /* Return result */
  return result;
}

#ifndef UNIKIT_DATA_H_INCLUDED
#define UNIKIT_DATA_H_INCLUDED

/*
 * unikit_data.h
 * =============
 * 
 * Data module of Unikit.
 * 
 * Clients do not need to use this module directly.  It is merely used
 * for storing encoded data tables that the main Unikit module needs.
 * 
 * The embedded base-64 data stored in this module is generated from the
 * Unicode Character Database using the unikit_db.pl script in the db
 * directory.  See the documentation of that script for further
 * information about the data format stored here.
 * 
 * Requirements
 * ------------
 * 
 *   none
 */

#include <stddef.h>

/*
 * The data keys that can be fetched.
 * 
 * The CASE constants are the case folding tables.  LOWER is the index
 * trie for the lower plane, UPPER is the index trie for the upper
 * plane, and DATA is the codepoint data array referenced from both
 * tries.
 * 
 * The GCAT constants are the data tables for General Category
 * information.  CORE is the core character table.  GENCHAR is the
 * general character table.  BITMAP is the bitmap character table.
 * ASTRAL is the astral character table.
 */
#define UNIKIT_DATA_KEY_CASE_LOWER    (100)
#define UNIKIT_DATA_KEY_CASE_UPPER    (101)
#define UNIKIT_DATA_KEY_CASE_DATA     (102)

#define UNIKIT_DATA_KEY_GCAT_CORE     (200)
#define UNIKIT_DATA_KEY_GCAT_GEN_LOW  (201)
#define UNIKIT_DATA_KEY_GCAT_GEN_HIGH (202) 
#define UNIKIT_DATA_KEY_GCAT_BITMAP   (203)
#define UNIKIT_DATA_KEY_GCAT_ASTRAL   (204)

/*
 * Fetch a base-64 string for a given data key.
 * 
 * The key must be one of the UNIKIT_DATA_KEY constants defined above.
 * NULL is returned if the key is not recognized.
 * 
 * Parameters:
 * 
 *   key - the data key to fetch
 * 
 * Return:
 * 
 *   the nul-terminated base-64 data string, or NULL if key not
 *   recognized
 */
const char *unikit_data_fetch(int key);

#endif

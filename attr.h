#pragma once
#ifndef RPL_ATTR_H
#define RPL_ATTR_H

#include "common.h"
#include "stringbuf.h"

//-------------------------------------------------------------
// text attributes
//-------------------------------------------------------------

#define RPL_ON   (1)
#define RPL_OFF  (-1)
#define RPL_NONE (0)

// try to fit in 64 bits 
// note: order is important for some compilers
// note: each color can actually be 25 bits
typedef union attr_s {
  struct {
    unsigned int  color:28;
    signed int    bold:2;
    signed int    reverse:2;
    unsigned int  bgcolor:28;
    signed int    underline:2;
    signed int    italic:2;
  } x;
  uint64_t        value;
} attr_t;

rpl_private attr_t attr_none(void);
rpl_private attr_t attr_default(void);
rpl_private attr_t attr_from_color( rpl_color_t color );

rpl_private bool attr_is_none(attr_t attr);
rpl_private bool attr_is_eq(attr_t attr1, attr_t attr2);

rpl_private attr_t attr_update_with( attr_t attr, attr_t newattr );

rpl_private attr_t attr_from_sgr( const char* s, ssize_t len);
rpl_private attr_t attr_from_esc_sgr( const char* s, ssize_t len);

//-------------------------------------------------------------
// attribute buffer used for rich rendering
//-------------------------------------------------------------

struct attrbuf_s;
typedef struct attrbuf_s attrbuf_t;

rpl_private attrbuf_t*     attrbuf_new( alloc_t* mem );
rpl_private void           attrbuf_free( attrbuf_t* ab );  // ab can be NULL
rpl_private void           attrbuf_clear( attrbuf_t* ab ); // ab can be NULL
rpl_private ssize_t        attrbuf_len( attrbuf_t* ab);    // ab can be NULL
rpl_private const attr_t*  attrbuf_attrs( attrbuf_t* ab, ssize_t expected_len );
rpl_private ssize_t        attrbuf_append_n( stringbuf_t* sb, attrbuf_t* ab, const char* s, ssize_t len, attr_t attr );

rpl_private void           attrbuf_set_at( attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr );
rpl_private void           attrbuf_update_at( attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr );
rpl_private void           attrbuf_insert_at( attrbuf_t* ab, ssize_t pos, ssize_t count, attr_t attr );

rpl_private attr_t         attrbuf_attr_at( attrbuf_t* ab, ssize_t pos );   
rpl_private void           attrbuf_delete_at( attrbuf_t* ab, ssize_t pos, ssize_t count );

#endif // RPL_ATTR_H

#include <stdlib.h>
#include <memory.h>
#include "rb.h"

typedef struct _rb_t {
    int unused;
} rb_t;

rb_handle
rb_create(
    void
    )
{
    rb_t * rb = malloc(sizeof(rb_t));
    memset(rb, 0, sizeof(rb_t));
    return rb;
}

void
rb_destroy(
    rb_handle rb
    )
{
    free(rb);
}

void
rb_insert(
    rb_handle rb,
    rb_value_t value
    )
{

}

rb_value_t
const
rb_find(
    rb_handle rb,
    rb_value_t value
    )
{
    return NULL;
}


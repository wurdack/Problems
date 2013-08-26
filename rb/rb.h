enum {
    rb_err_success = 0
};

typedef struct _rb_t * rb_handle;
typedef char * rb_value_t;

rb_handle
rb_create(
    void
    );

void
rb_destroy(
    rb_handle rb
    );

void
rb_insert(
    rb_handle rb,
    rb_value_t value
    );

rb_value_t
const
rb_find(
    rb_handle rb,
    rb_value_t value
    );

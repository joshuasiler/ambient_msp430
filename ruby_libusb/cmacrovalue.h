#include "ruby.h"

#define MACRO_MAP_CLASS(class_object) rb_define_class_variable(class_object, "@@macro_maps", rb_hash_new())
#define CLASS_MAPS(class_object) rb_cv_get(class_object, "@@macro_maps")
#define NEW_MACRO_MAP(class_object, name) rb_hash_aset(\
    CLASS_MAPS(class_object), \
    rb_str_new2(name), \
    rb_hash_new())
#define GET_MACRO_MAP(class_object, name) rb_hash_aref(CLASS_MAPS(class_object), rb_str_new2(name))
#define ADD_MACRO_MAPPING(class_object, name, macro) rb_hash_aset(GET_MACRO_MAP(class_object, name), \
    INT2NUM(macro), \
    rb_str_new2(#macro) \
    )
VALUE MACRO_MAPPED_VALUE(VALUE class_object, char* map_name, int value)
{
  return rb_funcall(class_object, rb_intern("create_cmacrovalue"), 2, INT2NUM(value), rb_str_new2(map_name));
}

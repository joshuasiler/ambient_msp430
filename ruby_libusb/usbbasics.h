#ifndef RUBY_LIBUSB_C_H
#define RUBY_LIBUSB_C_H

#include "ruby.h"

#define ENSURE_OPEN_BEGIN(device) \
  VALUE my_device = device; \
  VALUE is_open_ret = device_is_open( my_device ); \
  usb_dev_handle* handle; \
  if(is_open_ret == Qfalse) \
    { device_open( device ); } \
  Data_Get_Struct( rb_iv_get(device, "@open_handle") , usb_dev_handle, handle);

#define ENSURE_OPEN_END \
  if(is_open_ret == Qfalse) { device_close( my_device ); }

VALUE device_is_open(VALUE self);
VALUE device_open(VALUE self);
VALUE device_close(VALUE self);
void Init_usbbasics();
#endif

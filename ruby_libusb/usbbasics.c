#include "usbbasics.h"
#include "ruby.h"
#include "usb.h"
#include "stdio.h"
#include "cmacrovalue.h"
#include "errno.h"
#include <pthread.h>

#define ENSURE_OPEN_BEGIN(device) \
  VALUE my_device = device; \
  VALUE is_open_ret = device_is_open( my_device ); \
  usb_dev_handle* handle; \
  if(is_open_ret == Qfalse) \
    { device_open( device ); } \
  Data_Get_Struct( rb_iv_get(device, "@open_handle") , usb_dev_handle, handle);

#define ENSURE_OPEN_END \
  if(is_open_ret == Qfalse) { device_close( my_device ); }


VALUE cDevice, cEndpoint, cInterface, cConfiguration, cException, module;

/*
 * This is called implictly when you require libusb.  It gets all the current
 * devices.  It returns if true if the devices have changed since last load.
 *
 * You can use this function if you need to get any newly added usb devices, but
 * onece you use it stop using any old USB devoce objects you mights still have
 * around
 */
VALUE load_devices()
{
  int num_buses_changed = usb_find_busses();
  int num_devices_changed = usb_find_devices();
  if(num_buses_changed || num_devices_changed)
  {
    struct usb_bus *bus;
    struct usb_device *dev;
    VALUE result = rb_ary_new();
    for (bus = usb_busses; bus; bus = bus->next) {
      for (dev = bus->devices; dev; dev = dev->next) {
        VALUE new_device = Data_Wrap_Struct(cDevice, NULL, NULL, dev);
        rb_ary_push(result, new_device);
      }
    }
    rb_cv_set(module, "@@devices", result);
    return Qtrue;
  }
  return Qfalse;
}

/*
 * call-seq:
 *   filename() -> String
 *
 * Returns the libusb device attribute filename.  At least in linux, this seems
 * to correspond to the device's filename in /proc/bus/usb
 *
 */
VALUE device_filename(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return rb_str_new2(dev->filename);
}

/*
 * call-seq:
 *   dirname() -> String
 *
 * Returns the libusb device attribute dirname.  At least in linux, this seems
 * to correspond to the device's directory in /proc/bus/usb
 *
 */
VALUE device_dirname(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return rb_str_new2(dev->bus->dirname);
}

/*
 * call-seq:
 *   location() -> integer
 *
 * Returns the libusb device attribute location.  Don't know what the "location"
 * corresponds to.
 *
 */
VALUE device_location(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return INT2NUM(dev->bus->location);
}

/*
 * call-spec:
 * children() -> array of Device objects
 * Returns all the chidren devices of this device (which HUBs have)
 */
VALUE device_children(VALUE self)
{
  VALUE result = rb_ary_new();
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  unsigned char i;
  for(i = 0; i < dev->num_children; i++) {
    VALUE new_device = Data_Wrap_Struct(cDevice, NULL, NULL, dev->children[i]);
    rb_ary_push(result, new_device);
  }
  return result;
}

/*
 * Returns the device num.  This number is not unique - it may correspond to the
 * device's location in its particular hub but I'm not sure
 */
VALUE device_device_num(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return INT2NUM(dev->devnum);
}

VALUE array_for_bcd_value(u_int16_t value)
{
  VALUE result = rb_ary_new();
  rb_ary_push(result, INT2NUM(value >> 8));
  rb_ary_push(result, INT2NUM((value & 0x00F0) >> 4));
  rb_ary_push(result, INT2NUM(value & 0x000F));
  return result;
}

/*
 * Returns the USB version of this device as 3 element array (BCD value, as
 * described in the USB spec
 */
VALUE device_usb_version(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return array_for_bcd_value(dev->descriptor.bcdUSB);
}

/*
 * Returns the device version as a 3 element array (BCD value, as described in
 * the USB spec)
 */
VALUE device_device_version(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return array_for_bcd_value(dev->descriptor.bcdDevice);
}

/*
 * Returns the vendor ID for a device.
 */
VALUE device_vendor_id(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return INT2NUM(dev->descriptor.idVendor);
}

/*
 * Returns the product id for the device
 */
VALUE device_product_id(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return INT2NUM(dev->descriptor.idProduct);
}

/*
 * Returns the device class as a CMacroValue.  These classes are defined in the
 * USB spec although vendors can create their own.  Often devices set this value
 * to 0 and store the true value for this in the interfaces themselves
 */
VALUE device_device_class(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);

  return MACRO_MAPPED_VALUE(cDevice, "class_codes", dev->descriptor.bDeviceClass);
}

/* 
 * Returns the device subclass a number.  Certain device classes have subclass
 * meanings specified in the USB spec
 */
VALUE device_device_subclass(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);

  return INT2NUM(dev->descriptor.bDeviceSubClass);
}

/*
 * Returns the device protocol as a number.  This is often set to 0 but the USB
 * spec contains meaning for this, depending on the device class
 */
VALUE device_device_protocol(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);

  return INT2NUM(dev->descriptor.bDeviceProtocol);
}

/* 
 * This value determines the maximum size for the special control endpoint 0
 * that all USB devices must support.  You shouldn't have to use this endpoint
 * except through provided functions so this shouldn't be an issue
 */
VALUE device_max_packet_size_for_endpoint_0(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);

  return INT2NUM(dev->descriptor.bMaxPacketSize0);
}

void raise_usb_error()
{
  rb_raise(cException, usb_strerror());
}

/*
 * Returns true if the device is open.  In general, this library handles opening
 * and closing devices for you so this is something you should not need to check
 * on.
 */
VALUE device_is_open(VALUE self)
{
  if(rb_iv_get(self, "@open_handle") == Qnil)
    return Qfalse;
  return Qtrue;
}

/*
 * Opens the usb device.  The functions of libusb do this implictly when
 * necessary so in general this is something you shouldn't need to do by hand.
 * 
 * If you call open, you should call close
 */
VALUE device_open(VALUE self)
{
  if(device_is_open(self) == Qtrue)
  {
    rb_raise(cException, "USB device is already open");
  }
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  usb_dev_handle* handle = usb_open(dev);
  if(handle == NULL)
    raise_usb_error();
  rb_iv_set(self, "@open_handle", Data_Wrap_Struct(rb_cObject, NULL, NULL, handle));
  return Qnil;
}

/*
 * Closes the USB device.  In general libusb functions implictly close the
 * device as soon as they are done operating on them.  You can prevent this by
 * explictly using open if you want
 */
VALUE device_close(VALUE self)
{
  if(device_is_open(self) == Qfalse)
  {
    rb_raise(cException, "Attempting to close an already closed USB device");
  }
  usb_dev_handle* handle;
  Data_Get_Struct(rb_iv_get(self, "@open_handle"), usb_dev_handle, handle);
  rb_iv_set(self, "@open_handle", Qnil);
  if(usb_close(handle) < 0)
    raise_usb_error();
  return Qnil;
}

VALUE get_string_index(VALUE dev, int index)
{
  char string[100];
  if(index == 0) return rb_str_new2("");
  ENSURE_OPEN_BEGIN(dev);
  int result = usb_get_string_simple(handle, index, string, 100);
  ENSURE_OPEN_END;
  if(result < 0) {
    raise_usb_error();
  }
  return rb_str_new2(string);
}

/*
 * This function gets the string serial number for the device.  Note that this
 * requires an explict call to the device which requires appropiate permissions
 * to succeed.  This function gets the string in the 1st language specified by
 * the device.  According to the USB spec, this is an UTF8 string.
 */
VALUE device_serial_number(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return get_string_index(self, dev->descriptor.iSerialNumber);
}

/*
 * This function gets the string manufacturer name for the device.  Note that this
 * requires an explict call to the device which requires appropiate permissions
 * to succeed.  This function gets the string in the 1st language specified by
 * the device.  According to the USB spec, this is an UTF8 string.
 */
VALUE device_manufacturer(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return get_string_index(self, dev->descriptor.iManufacturer);
}

/*
 * This function gets the string product name for the device.  Note that this
 * requires an explict call to the device which requires appropiate permissions
 * to succeed.  This function gets the string in the 1st language specified by
 * the device.  According to the USB spec, this is an UTF8 string.
 */
VALUE device_product_name(VALUE self)
{
  struct usb_device *dev;
  Data_Get_Struct(self, struct usb_device, dev);
  return get_string_index(self, dev->descriptor.iProduct);
}

/*
 * This returns the current configurtion number for the device.  This requires a
 * explict call to the device so the calling program must have appropiate
 * permissions.
 */
VALUE device_current_configuration_value(VALUE self)
{
  ENSURE_OPEN_BEGIN(self);
  char data;
  int result = (usb_control_msg(handle, 0x80, USB_REQ_GET_CONFIGURATION, 0, 0, &data, 1, 1000));
  ENSURE_OPEN_END;
  if(result < 0)
    raise_usb_error();
  if(result != 1)
    rb_raise(cException, "Unexpected length %d from GET_CONFIGURATION", result);
  return INT2NUM(data);
}

/*
 * Returns the current alternative number, which determines which alternate interface
 * is active within all the interfaces of the same value.  Generally it's easier
 * to call current_alternative?
 */
VALUE interface_current_alternative(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  char data;
  
  ENSURE_OPEN_BEGIN(rb_iv_get(self, "@device"));
  int result = usb_control_msg(handle, 0x80, USB_REQ_GET_INTERFACE, 0, interface->bInterfaceNumber, &data, 1, 1000);
  ENSURE_OPEN_END;

  if(result < 0)
    raise_usb_error();
  if(result != 1)
    rb_raise(cException, "Unexpected length %d from GET_INTERFACE", result);

  return INT2NUM(data);
}

/*
 * Returns the interface class as a CMacroValue.  Some valid classes are:
 * USB_CLASS_PER_INTERFACE USB_CLASS_AUDIO USB_CLASS_COMM USB_CLASS_HID 
 * USB_CLASS_PRINTER USB_CLASS_PTP USB_CLASS_MASS_STORAGE USB_CLASS_HUB
 * USB_CLASS_DATA USB_CLASS_VENDOR_SPEC
 *
 *   if(interface.interface_class.string_value == "USB_CLASS_HID") 
 *     puts "HID Device"
 *   end
 */
VALUE interface_interface_class(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return MACRO_MAPPED_VALUE(cDevice, "class_codes", interface->bInterfaceClass);
}

/*
 * The alternate setting for this interface.  When the current alternative is
 * set to this number, this particular interface is active
 */
VALUE interface_alternate_setting(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return INT2NUM(interface->bAlternateSetting);
}

/*
 * The numer of this interface.  Interfaces may have the same value - in that
 * case they are differentiated by their alternate setting and only 1 interface
 * may be active at a particular time
 */
VALUE interface_number(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return INT2NUM(interface->bInterfaceNumber);
}

/*
 * Gets the subclass with this interface as an int.  This is often 0 but certian
 * USB speced interface classes assocaite a particular meaning with this value
 */
VALUE interface_subclass(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return INT2NUM(interface->bInterfaceSubClass);
}

/*
 * Gets the protocol assicated with this interface as an int.  This is often 0 but certian
 * USB speced interface classes assocaite a particular meaning with this value
 */
VALUE interface_protocol(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return INT2NUM(interface->bInterfaceProtocol);
}

/* 
 * Don't use this.  Call endpoints instead
 */
VALUE interface_endpoints(VALUE self)
{
  VALUE result = rb_ary_new();
  struct usb_interface_descriptor* interface;
  int e;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  for(e = 0; e < interface->bNumEndpoints; e++) {
    struct usb_endpoint_descriptor *endpoint = interface->endpoint + e;
    VALUE new_endpoint = Data_Wrap_Struct(cEndpoint, NULL, NULL, endpoint);
    rb_iv_set(new_endpoint, "@interface", self);
    rb_iv_set(new_endpoint, "@device", rb_iv_get(self, "@device"));
    rb_ary_push(result, new_endpoint);
  }
  return result;
}

/* 
 * USB interfaces may optionally have a string discriptor assoicated with them.
 * This returns that descriptor
 */
VALUE interface_interface_string(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  return get_string_index(rb_iv_get(self, "@device"), interface->iInterface);
}

/*
 * Attempts to briefly claim the interface and returns false if it fails.  You
 * must claim an interface before you can get interrupts from it.  One of the
 * reasons an interface claim may fail is that you need to detach_kernel
 */
VALUE interface_is_claimable(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  ENSURE_OPEN_BEGIN(rb_iv_get(self, "@device"));
  if(usb_claim_interface(handle, interface->bInterfaceNumber) < 0) {
    ENSURE_OPEN_END;
    return Qfalse;
  } else {
    if(usb_release_interface(handle, interface->bInterfaceNumber) < 0) {
      ENSURE_OPEN_END;
      raise_usb_error();
    }
  }
  ENSURE_OPEN_END;
  return Qtrue;
}

/*
 * detaches the kernel from a particular USB interface.  The kernel tends to
 * claim HID devices it recognises, so if you want to utilitize the device
 * directly through libusb you must first detach the kernel.  This stop the
 * device from acting like a regular linux mouse or keyboard and let you utilize
 * it directly
 */
VALUE interface_detach_kernel(VALUE self)
{
  if(interface_is_claimable(self) == Qtrue) return Qnil;

  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  ENSURE_OPEN_BEGIN(rb_iv_get(self, "@device"));
  int result = usb_detach_kernel_driver_np(handle, interface->bInterfaceNumber);
  ENSURE_OPEN_END;
  if(result < 0) {
    raise_usb_error();
  }
  return Qnil;
}

/*
 * Load the configurtions.  Don't use this function - use configurtions instead
 */
VALUE device_configurations(VALUE self)
{
  VALUE result = rb_ary_new();
  struct usb_device *dev;
  int c;
  Data_Get_Struct(self, struct usb_device, dev);
  for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
    VALUE new_configuration = Data_Wrap_Struct(cConfiguration, NULL, NULL, dev->config + c);
    rb_iv_set(new_configuration, "@device", self);
    rb_ary_push(result, new_configuration);
  }
  return result;
}


/* 
 * Don't use this - call interfaces instead
 */
VALUE configuration_interfaces(VALUE self) 
{
  VALUE result = rb_ary_new();
  struct usb_config_descriptor* config;
  int i, a;
  Data_Get_Struct(self, struct usb_config_descriptor, config);
  for(i = 0; i < config->bNumInterfaces; i++) {
    /* Loop through all of the alternate settings */
    for (a = 0; a < config->interface[i].num_altsetting; a++) {
      VALUE new_interface = Data_Wrap_Struct(cInterface, NULL, NULL, config->interface[i].altsetting + a);
      rb_iv_set(new_interface, "@device", rb_iv_get(self, "@device"));
      rb_iv_set(new_interface, "@configuration", self);
      rb_ary_push(result, new_interface);
    }
  }
  return result;
}

/*
 * Returns the ID of the configuration
 */
VALUE configuration_value(VALUE self)
{
  struct usb_config_descriptor* config;
  Data_Get_Struct(self, struct usb_config_descriptor, config);
  return INT2NUM(config->bConfigurationValue);
}

/*
 * Return the max power drawn from the device in milliamp units (not two
 * milliamp units like described in the USB spec
 */
VALUE configuration_max_power(VALUE self)
{
  struct usb_config_descriptor* config;
  Data_Get_Struct(self, struct usb_config_descriptor, config);
  return INT2NUM(config->MaxPower * 2);
}

/* 
 * Returns true if the configuration is self powered, false if the device 
 * is bus powered 
 */
VALUE configuration_is_self_powered(VALUE self)
{
  struct usb_config_descriptor* config;
  Data_Get_Struct(self, struct usb_config_descriptor, config);
  if(config->bmAttributes & (1 << 6))
    return Qtrue;
  else
    return Qfalse;
}

/* 
 * Returns true if the device supports the remote wakeup feature, which a allows
 * a USB device to signal a sleeping machine to wake up.
 */
VALUE configuration_supports_remote_wakeup(VALUE self)
{
  struct usb_config_descriptor* config;
  Data_Get_Struct(self, struct usb_config_descriptor, config);
  if(config->bmAttributes & (1 << 5))
    return Qtrue;
  else
    return Qfalse;
}

/* 
 * This value's meaning depends on endpoint type.  It could be the max latency for polling interupt endpoints, the
 * interval for polling isync endpoints, or max NAK rate for bulk OUT/ control endpoints
 */
VALUE endpoint_interval(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);

  return INT2NUM(e->bInterval);
}

/*
 * Returns the max packet size for the endpoing
 */
VALUE endpoint_max_packet_size(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);

  return INT2NUM(0x07FF & e->wMaxPacketSize);
}

/*
 * Returns the type of endpoint as a CMacroValue.  Types of endpoints include:
 *
 * USB_ENDPOINT_TYPE_CONTROL, USB_ENDPOINT_TYPE_ISOCHRONOUS, USB_ENDPOINT_TYPE_BULK, 
 * USB_ENDPOINT_TYPE_INTERRUPT
 */
VALUE endpoint_type(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);
  return MACRO_MAPPED_VALUE(cDevice, "endpoint_types", e->bmAttributes & USB_ENDPOINT_TYPE_MASK);
}

/*
 * Returns the number of the endpoint.  Note that it's possible for 2 endpoints
 * to share the same endpoint number (one input endpoint, one output endpoint).
 */
VALUE endpoint_number(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);
  return INT2NUM(USB_ENDPOINT_ADDRESS_MASK & e->bEndpointAddress);
}

/*
 * Returns true if the endpoint is an output endpoint (input in this cases, means
 * from the perspective of the host so output endpoints transmit data from the
 * computer to usb devices
 *
 * Control endpoints return false.
 */
VALUE endpoint_is_output(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);
  if((e->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL)
    return Qfalse;
  if(USB_ENDPOINT_DIR_MASK & e->bEndpointAddress)
    return Qfalse;
  else
    return Qtrue;
}

/*
 * Returns true if the endpoint is an input endpoint (input in this cases, means
 * from the perspective of the host so input endpoints transmit data from usb
 * devices to the computer)
 *
 * Control endpoints return false.
 */
VALUE endpoint_is_input(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);
  if((e->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_CONTROL)
    return Qfalse;
  if(USB_ENDPOINT_DIR_MASK & e->bEndpointAddress)
    return Qtrue;
  else
    return Qfalse;
}

struct interrupt_result_list_entry {
  int data_size;
  struct interrupt_result_list_entry* next;
  char data[0];
};

struct interrupt_result_list {
  struct interrupt_result_list_entry *head, *tail;
  pthread_mutex_t mutex;
};

void free_data(void* data)
{
  free(data);
}
  
void mutex_unlock(void *mutex)
{
  pthread_mutex_unlock((pthread_mutex_t*) mutex);
}

struct interrupt_thread_parameters
{
  u_int8_t bEndpointAddress;
  u_int16_t wMaxPacketSize;
  usb_dev_handle* handle;
  struct interrupt_result_list* list;
};

void add_to_list(struct interrupt_result_list* list, struct interrupt_result_list_entry* list_item)
{
  pthread_cleanup_push(mutex_unlock, &(list->mutex));
  pthread_mutex_lock(&(list->mutex));
  if(list->tail == NULL) {
    list->head = list_item;
    list->tail = list_item;
  } else {
    list->tail->next = list_item;
    list->tail = list_item;
  }
  pthread_mutex_unlock(&(list->mutex));
  pthread_cleanup_pop(0);
}

/* 
 * this is the function that actually listens in a seperate thread for
 * interrupts as they come in
 */
void* test_thread(void *args)
{
  struct interrupt_thread_parameters* params = (struct interrupt_thread_parameters *) args;
  u_int8_t endpoint_address = params->bEndpointAddress;
  u_int16_t max_size = params->wMaxPacketSize;
  usb_dev_handle* handle = params->handle;
  struct interrupt_result_list* list = params->list;

  free(params);
  char* data = malloc(max_size);
  pthread_cleanup_push(free_data, data);
  int result;
  while(1) {
    result = usb_interrupt_read(handle, endpoint_address, data, max_size, 0);

    if(result < 0)
      break;
    struct interrupt_result_list_entry* list_item = (struct interrupt_result_list_entry*) malloc(sizeof(struct interrupt_result_list_entry) + result);
    // printf("buffalo ");
    // for(i = 0; i < result; i++)
    //   printf(" %x", data[i]);
    // printf("\n");
    memcpy(list_item->data, data, result);
    list_item->data_size = result;
    list_item->next = NULL;
    add_to_list(list, list_item);
  }
  //an error occured
  pthread_cleanup_pop(0);
  free(data);
  static struct interrupt_result_list_entry error_item;
  error_item.data_size = -1; //negative size indicates an error
  error_item.next = NULL;
  add_to_list(list, &error_item);
  return NULL;
}


/*
 * Returns the next available interrupt as a ruby string or returns nil if no
 * interrupt is currentl available
 *
 * This function should generally be used with a listen_for_interrupts block.
 * It is possible to call it after the block has ended and return interrupts
 * that were received while the block was open but not porcessed.
 */
VALUE endpoint_next_interrupt_or_nil(VALUE self)
{
  struct interrupt_result_list* list;
  if(rb_iv_get(self, "@result_list") == Qnil) return Qnil;
  Data_Get_Struct(rb_iv_get(self,"@result_list"), struct interrupt_result_list, list);
  if(list->head == NULL) {
    return Qnil;
  }
  struct interrupt_result_list_entry* data = list->head;
  pthread_mutex_lock(&(list->mutex));
  list->head = list->head->next;
  if(list->head == NULL) {
    list->tail = NULL;
  }
  pthread_mutex_unlock(&(list->mutex));
  if(data->data_size < 0) {
    //an error has occured
    raise_usb_error();
  }

  VALUE result = rb_str_new(data->data, data->data_size);
  return result;
}

void free_interrupt_list(void * interrupt_list)
{
  struct interrupt_result_list* list = (struct interrupt_result_list*) interrupt_list;
  struct interrupt_result_list_entry* previous;
  struct interrupt_result_list_entry* current = list->head;
  while(current != NULL) {
    previous = current;
    current = current->next;
    free(previous);
  }
  free(list);
}

/*
 * Under most circustances it's better to use listen_for_interrupts than use
 * this methods (just because it ensure that the device is closed when you're
 * done.  But this starts a device listening, and from there you can use
 * next_interrupt to get the data.  You must call interrupt_close if you call
 * interrupt_open
 */
VALUE endpoint_start_listening(VALUE self)
{
  struct usb_endpoint_descriptor* e;
  Data_Get_Struct(self, struct usb_endpoint_descriptor, e);
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(rb_iv_get(self,"@interface"), struct usb_interface_descriptor, interface);
  struct interrupt_result_list* list;
  if(rb_iv_get(self, "@result_list") == Qnil) {
    // this initializes the structure the thread will use to communicate
    list = malloc(sizeof(struct interrupt_result_list));
    list->head = NULL;
    list->tail = NULL;
    pthread_mutex_init(&list->mutex, 0);
    rb_iv_set(self, "@result_list", Data_Wrap_Struct(rb_cObject, NULL, free_interrupt_list, list));
  } else {
    Data_Get_Struct(rb_iv_get(self,"@result_list"), struct interrupt_result_list, list);
  }
  Data_Get_Struct(rb_iv_get(self,"@interface"), struct usb_interface_descriptor, interface);


  ENSURE_OPEN_BEGIN(rb_iv_get(self, "@device"));
  if(usb_claim_interface(handle, interface->bInterfaceNumber) < 0) {
    ENSURE_OPEN_END;
    raise_usb_error();
  }
  pthread_t* thread1 = malloc(sizeof(pthread_t));
  int ret;
  struct interrupt_thread_parameters* thread_params = malloc(sizeof(struct interrupt_thread_parameters));
  thread_params->bEndpointAddress = e->bEndpointAddress;
  thread_params->wMaxPacketSize = e->wMaxPacketSize;
  thread_params->handle = handle;
  thread_params->list = list;
  ret = pthread_create(thread1, NULL, test_thread, thread_params);
  rb_iv_set(self, "@thread", Data_Wrap_Struct(rb_cObject, NULL, NULL, thread1));
  rb_iv_set(rb_iv_get(self, "@interface"), "@claimed", Qtrue);
  return Qnil;
}

/* 
 * Under most circumstances it's better to use listen_for_interrputs than this
 * method.  But if you call start_listening you must call stop_listening
 */
VALUE endpoint_stop_listening(VALUE self)
{
  char error_occured = 0;
  struct usb_dev_handle *handle;
  Data_Get_Struct(rb_iv_get(rb_iv_get(self, "@device"),"@open_handle"), struct usb_dev_handle, handle);
  struct usb_interface_descriptor* interface;
  
  Data_Get_Struct(rb_iv_get(self,"@interface"), struct usb_interface_descriptor, interface);
  pthread_t* thread;
  Data_Get_Struct(rb_iv_get(self, "@thread"), pthread_t, thread);
  
  pthread_cancel(*thread);
  free(thread);
  rb_iv_set(self, "@thread", Qnil);

  if(usb_release_interface(handle, interface->bInterfaceNumber) < 0) {
    error_occured = 1;
  }
  rb_iv_set(rb_iv_get(self, "@interface"), "@claimed", Qfalse);
  device_close(rb_iv_get(self, "@device"));
  if(error_occured)
    raise_usb_error();
  return Qnil;
}


void Init_usbbasics() {
  module = rb_define_module("USB");
  cException = rb_define_class_under(module, "UsbException", rb_eStandardError);
  cDevice = rb_define_class_under(module, "Device", rb_cObject);
  rb_define_module_function(module, "load_devices", load_devices, 0);

  rb_define_method(cDevice, "filename", device_filename, 0);
  rb_define_method(cDevice, "dirname", device_dirname, 0);
  rb_define_method(cDevice, "device_num", device_device_num, 0);
  rb_define_method(cDevice, "location", device_location, 0);
  rb_define_method(cDevice, "children", device_children, 0);
  rb_define_method(cDevice, "usb_version", device_usb_version, 0);
  rb_define_method(cDevice, "device_version", device_device_version, 0);
  rb_define_method(cDevice, "vendor_id", device_vendor_id, 0);
  rb_define_method(cDevice, "product_id", device_product_id, 0);
  rb_define_method(cDevice, "device_class", device_device_class, 0);
  rb_define_method(cDevice, "device_subclass", device_device_subclass, 0);
  rb_define_method(cDevice, "device_protocol", device_device_protocol, 0);
  rb_define_method(cDevice, "product_name", device_product_name, 0);
  rb_define_method(cDevice, "manufacturer", device_manufacturer, 0);
  rb_define_method(cDevice, "serial_number", device_serial_number, 0);
  rb_define_method(cDevice, "max_packet_size_for_endpoint_0", device_max_packet_size_for_endpoint_0, 0);
  rb_define_method(cDevice, "load_configurations", device_configurations, 0);
  rb_define_method(cDevice, "current_configuration_value", device_current_configuration_value, 0);
  rb_define_method(cDevice, "open?", device_is_open, 0);
  rb_define_method(cDevice, "open", device_open, 0);
  rb_define_method(cDevice, "close", device_close, 0);
  cEndpoint = rb_define_class_under(module, "Endpoint", rb_cObject);
  rb_define_method(cEndpoint, "type", endpoint_type, 0);
  rb_define_method(cEndpoint, "output?", endpoint_is_output, 0);
  rb_define_method(cEndpoint, "input?", endpoint_is_input, 0);
  rb_define_method(cEndpoint, "number", endpoint_number, 0);
  rb_define_method(cEndpoint, "max_packet_size", endpoint_max_packet_size, 0);
  rb_define_method(cEndpoint, "interval", endpoint_interval, 0);
  rb_define_method(cEndpoint, "start_interrupt_listening", endpoint_start_listening, 0);
  rb_define_method(cEndpoint, "stop_interrupt_listening", endpoint_stop_listening, 0);
  rb_define_method(cEndpoint, "next_interrupt_or_nil", endpoint_next_interrupt_or_nil, 0);
  cInterface = rb_define_class_under(module, "Interface", rb_cObject);
  rb_define_method(cInterface, "load_endpoints", interface_endpoints, 0);
  rb_define_method(cInterface, "interface_class", interface_interface_class, 0);
  rb_define_method(cInterface, "current_alternative", interface_current_alternative, 0);
  rb_define_method(cInterface, "interface_string", interface_interface_string, 0);
  rb_define_method(cInterface, "subclass", interface_subclass, 0);
  rb_define_method(cInterface, "protocol", interface_protocol, 0);
  rb_define_method(cInterface, "alternate_setting", interface_alternate_setting, 0);
  rb_define_method(cInterface, "number", interface_number, 0);
  rb_define_method(cInterface, "claimable?", interface_is_claimable, 0);
  rb_define_method(cInterface, "detach_kernel", interface_detach_kernel, 0);
  cConfiguration = rb_define_class_under(module, "Configuration", rb_cObject);
  rb_define_method(cConfiguration, "load_interfaces", configuration_interfaces, 0);
  rb_define_method(cConfiguration, "value", configuration_value, 0);
  rb_define_method(cConfiguration, "self_powered?", configuration_is_self_powered, 0);
  rb_define_method(cConfiguration, "remote_wakeup?", configuration_supports_remote_wakeup, 0);
  rb_define_method(cConfiguration, "max_power", configuration_max_power, 0);

  MACRO_MAP_CLASS(cDevice);
  NEW_MACRO_MAP(cDevice, "class_codes");
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_PER_INTERFACE);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_AUDIO);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_COMM);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_HID);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_PRINTER);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_PTP);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_MASS_STORAGE);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_HUB);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_DATA);
  ADD_MACRO_MAPPING(cDevice, "class_codes", USB_CLASS_VENDOR_SPEC);

  NEW_MACRO_MAP(cDevice, "endpoint_types");
  ADD_MACRO_MAPPING(cDevice, "endpoint_types", USB_ENDPOINT_TYPE_CONTROL);
  ADD_MACRO_MAPPING(cDevice, "endpoint_types", USB_ENDPOINT_TYPE_ISOCHRONOUS);
  ADD_MACRO_MAPPING(cDevice, "endpoint_types", USB_ENDPOINT_TYPE_BULK);
  ADD_MACRO_MAPPING(cDevice, "endpoint_types", USB_ENDPOINT_TYPE_INTERRUPT);
  
  usb_init();
}

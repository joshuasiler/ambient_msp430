/*  Copyright 2006, Michael Hewner
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 of the License, 
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <usb.h>
#include "ruby.h"
#include "rubyio.h"
extern "C" {
#include "usbbasics.h"
}
#include <stdio.h>
#include "ReportDescriptor.h"
#include "cmacrovalue.h"

typedef VALUE (ruby_method)(...);

using namespace HID;
using namespace std;

VALUE cException, cReportDescriptor, cReport, cInputOutputFeature, cUsage;

void raise_usb_error()
{
  rb_raise(cException, usb_strerror());
}

extern "C" static void descriptor_free(void *p)
{
  delete static_cast<ReportDescriptor*>(p);
}

extern "C" static void usage_free(void *p)
{
  delete static_cast<Usage*>(p);
}

/*
 * Document-method: inputs
 * Returns all the input InputOutputFeatures
 */
extern "C" VALUE report_inputs(VALUE self)
{
  VALUE result = rb_ary_new();
  Report* r;
  Data_Get_Struct(self, Report, r);
  vector<const InputOutputFeature*>::const_iterator i;
  for(i = r->inputs().begin(); i != r->inputs().end(); i++) {
    rb_ary_push(result, Data_Wrap_Struct(cInputOutputFeature, NULL, NULL, const_cast<InputOutputFeature*>(*i)));
  }
  return result;
}

/*
 * Document-method: outputs
 * Returns all the output and feature InputOutputFeatures
 */
extern "C" VALUE report_outputs(VALUE self)
{
  VALUE result = rb_ary_new();
  Report* r;
  Data_Get_Struct(self, Report, r);
  vector<const InputOutputFeature*>::const_iterator i;
  for(i = r->outputsAndFeatures().begin(); i != r->outputsAndFeatures().end(); i++) {
    rb_ary_push(result, Data_Wrap_Struct(cInputOutputFeature, NULL, NULL, const_cast<InputOutputFeature*>(*i)));
  }
  return result;
}

/*
 * Document-method: id
 * Returns the report_id of the report
 */
extern "C" VALUE report_id(VALUE self)
{
  Report* r;
  Data_Get_Struct(self, Report, r);
  return INT2NUM(r->reportId());
}

/*
 * Document-method: load_reports
 * Don't use this method.  Call reports instead
 */
extern "C" VALUE report_descriptor_reports(VALUE self)
{
  VALUE result = rb_ary_new();
  ReportDescriptor* rd;
  Data_Get_Struct(self, ReportDescriptor, rd);
  map<unsigned char, Report>::const_iterator i = rd->reports().begin();
  while(i != rd->reports().end()) {
    const Report* report = &i->second;
    rb_ary_push(result, Data_Wrap_Struct(cReport, NULL, NULL, const_cast<Report*>(report)));
    i++;
  }
  return result;
}

/*
 * Document-method: report_size
 *
 * The bit length of an individual item in this Input, Output or Feature.  The
 * length is this times the report_count
 */
extern "C" VALUE inputoutputfeature_report_size(VALUE self)
{
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  return ULONG2NUM(r->reportSize());
}

/*
 * Document-method: report_count
 *
 * The number of items in this report.  For a array field, this limits the
 * number of items that can be reported at once.  For a variable field this
 * determines the number of variables
 */
extern "C" VALUE inputoutputfeature_report_count(VALUE self)
{
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  return ULONG2NUM(r->reportCount());
}

/*
 * Document-method: length
 * The total length of this input/output/feature in a reportdatagram in bits
 */
extern "C" VALUE inputoutputfeature_length(VALUE self)
{
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  return ULONG2NUM(r->length());
}

extern "C" VALUE inputoutputfeature_is_flag_set_internal(VALUE self, VALUE flagEnum)
{
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  if(r->hasFlag((InputOutputFeature::BitFlagEnum) NUM2INT(flagEnum)))
    return Qtrue;
  return Qfalse;
}

/*
 * Document-method: usage_for_field
 *
 * This method takes an int and returns the usage corresponding to the ith field
 * in the report.  For variable items this is the ith entry in the datagram.
 * For array items it is the usage for the value i in the array return
 */
extern "C" VALUE inputoutputfeature_usage_for_field(VALUE self, VALUE field_num)
{
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  Usage u = r->usageForField(NUM2ULONG(field_num));
  if(!u.isValid()) return Qnil;
  Usage *usage_for_field = new Usage(u);
  return Data_Wrap_Struct(cUsage, NULL, usage_free, usage_for_field);
}

/* Document-method: all_usages
 * Returns all usages the device is capable of returning
 */
extern "C" VALUE inputoutputfeature_all_usages(VALUE self)
{
  VALUE result = rb_ary_new();
  InputOutputFeature* r;
  Data_Get_Struct(self, InputOutputFeature, r);
  vector<Usage> usages = r->allUsages();
  vector<Usage>::const_iterator i;
  for(i = usages.begin(); i != usages.end(); i++) {
    Usage* usage = new Usage(*i);
    rb_ary_push(result, Data_Wrap_Struct(cUsage, NULL, usage_free, usage));
  }
  return result;
}

extern "C" VALUE usage_create_usage(VALUE self, VALUE usage_page, VALUE usage)
{
  Usage* new_usage = new Usage(NUM2INT(usage_page), NUM2INT(usage));
  return Data_Wrap_Struct(cUsage, NULL, usage_free, new_usage);
}

/* 
 * Document-method: usage
 * Returns the number corresponding to the usage portion of the extendend usage
 * (that is, this does not include the usage page)
 */
extern "C" VALUE usage_usage(VALUE self)
{
  Usage* u;
  Data_Get_Struct(self, Usage, u);
  return INT2NUM(u->usage());
}

/*
 * Document-method: usage_page
 * Returns the number of the usage_page for this usage
 */
extern "C" VALUE usage_usage_page(VALUE self)
{
  Usage* u;
  Data_Get_Struct(self, Usage, u);
  return INT2NUM(u->usagePage());
}

/*
 * Document-method: extended_usage
 * Returns a single numbere containing both the usage and the usage page
 */
extern "C" VALUE usage_extended_usage(VALUE self)
{
  Usage* u;
  Data_Get_Struct(self, Usage, u);
  return INT2NUM(u->extendedUsage());
}

extern "C" VALUE usage_name(VALUE self)
{
  Usage* u;
  Data_Get_Struct(self, Usage, u);
  return rb_str_new2(u->usageName().c_str());
}

extern "C" VALUE usage_page_name(VALUE self)
{
  Usage* u;
  Data_Get_Struct(self, Usage, u);
  return rb_str_new2(u->usagePageName().c_str());
}

/*
 * Document-method: load_hid_report_descriptor
 * Do not use this.  Use hid_report_descriptor
 */
extern "C" VALUE interface_hid_report_descriptor(VALUE self)
{
  struct usb_interface_descriptor* interface;
  Data_Get_Struct(self, struct usb_interface_descriptor, interface);
  char data[9];
  int result;
  ENSURE_OPEN_BEGIN(rb_iv_get(self, "@device"));
  result = usb_control_msg(handle, 0x81, USB_REQ_GET_DESCRIPTOR, USB_DT_HID << 8, interface->bInterfaceNumber, data, 9, 1000);
  if(result < 0)
  {
    ENSURE_OPEN_END;
    raise_usb_error();
  }
  if(result != 9)
  {
    ENSURE_OPEN_END;
    rb_raise(cException, "HID descriptor returned sucessfully but is wrong size: expected %d chars got %d", 9, result);
  }

  int report_descriptor_size = (unsigned char) data[7] | ((unsigned char) data[8] << 8);
  char* descriptor_array = (char*) malloc(report_descriptor_size);
  result = usb_control_msg(handle, 0x81, USB_REQ_GET_DESCRIPTOR, USB_DT_REPORT << 8, interface->bInterfaceNumber, descriptor_array, report_descriptor_size, 2000);
  ENSURE_OPEN_END;
  if(result < 0)
  {
    raise_usb_error();
  }
  if(result != report_descriptor_size)
  {
    rb_raise(cException, "HID report returned sucessfully but is wrong size: expected %d chars got %d", report_descriptor_size, result);
  }

  ReportDescriptor* myDesc = new ReportDescriptor((unsigned char*) descriptor_array, result);
  free(descriptor_array);
  VALUE return_val = Data_Wrap_Struct(cReportDescriptor, NULL, descriptor_free, myDesc);
  return return_val;
} 

extern "C" void Init_hid() {
  VALUE module = rb_define_module("USB");
  // this class is already defined in the libusb ruby file
  // but I'm adding some hid-specific operations to it
  VALUE cInterface = rb_define_class_under(module, "Interface", rb_cObject);

  //normally I would not do something tricky like this.  But without this rdoc
  //stops working properly
# define rb_define_method(rClass, rString, cFunct, rest...) rb_define_method(rClass, rString, (ruby_method*) cFunct, rest)

  rb_define_method(cInterface, "load_hid_report_descriptor",  interface_hid_report_descriptor, 0);
  cException = rb_define_class_under(module, "UsbException", rb_eStandardError);
  cReportDescriptor = rb_define_class_under(module, "ReportDescriptor", rb_cObject);
  rb_define_method(cReportDescriptor, "load_reports",  report_descriptor_reports, 0);
  cReport = rb_define_class_under(module, "Report", rb_cObject);
  rb_define_method(cReport, "inputs",  report_inputs, 0);
  rb_define_method(cReport, "outputs",  report_outputs, 0);
  rb_define_method(cReport, "id",  report_id, 0);
  cInputOutputFeature = rb_define_class_under(module, "InputOutputFeature", rb_cObject);
  rb_define_method(cInputOutputFeature, "all_usages",  inputoutputfeature_all_usages, 0);
  rb_define_method(cInputOutputFeature, "length",  inputoutputfeature_length, 0);
  rb_define_method(cInputOutputFeature, "report_count",  inputoutputfeature_report_count, 0);
  rb_define_method(cInputOutputFeature, "report_size",  inputoutputfeature_report_size, 0);
  rb_define_method(cInputOutputFeature, "usage_for_field",  inputoutputfeature_usage_for_field, 1);
  rb_define_method(cInputOutputFeature, "flag_set_internal?",  inputoutputfeature_is_flag_set_internal, 1);
  cUsage = rb_define_class_under(module, "Usage", rb_cObject);
  rb_define_module_function(cUsage, "create_usage",  (ruby_method*) usage_create_usage, 2);
  rb_define_method(cUsage, "usage",  usage_usage, 0);
  rb_define_method(cUsage, "usage_page",  usage_usage_page, 0);
  rb_define_method(cUsage, "extended_usage",  usage_extended_usage, 0);
  rb_define_method(cUsage, "usage_name",  usage_name, 0);
  rb_define_method(cUsage, "usage_page_name", usage_page_name, 0);

  VALUE cDevice = rb_define_class_under(module, "Device", rb_cObject);
  NEW_MACRO_MAP(cDevice, "data_item_flags");
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::DATA);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::CONSTANT);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::ARRAY);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::VARIABLE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::ABSOLUTE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::RELATIVE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NO_WRAP);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::WRAP);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::LINEAR);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NONLINEAR);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::PREFERRED_STATE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NO_PREFERRED_STATE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NULL_STATE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NO_NULL_STATE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::NONVOLATILE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::VOLATILE);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::BIT_FIELD);
  ADD_MACRO_MAPPING(cDevice, "data_item_flags", InputOutputFeature::BUFFERED_BYTES);
  
}


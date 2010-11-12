
#include "usbbasics.h"
#include "hid.h"

void Init_libusb_c() {
  Init_usbbasics();
  Init_hid();
}

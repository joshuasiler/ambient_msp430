require 'mkmf'

find_library("usb", nil, "/usr/local/lib")
# create_makefile("libusb_c")

# run like this...
# ruby extconf.rb --with-hidparse-dir=/home/hewner/buffaloplay/HIDParser

dir_config("hidparse", true)
have_library("hidparse")
create_makefile("libusb_c")

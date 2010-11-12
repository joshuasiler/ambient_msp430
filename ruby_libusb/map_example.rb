# An example application that finds the first HID device and listens for changes
# to the "K" keyboard key

require 'libusb'


hid_device = USB::devices.select { |d| d.hid? }[0]
hid_interface = hid_device.interfaces { |i| i.hid? }[0]
hid_interface.detach_kernel
listener = USB::UsageListener.new(hid_interface)
usage = USB::Usage.find_usage_named(/Keyboard f/)
puts "listening for usage #{usage}"
listener.on_usage_value_change(usage) { |new_val| puts "New value: #{new_val}" }
listener.start_listening 

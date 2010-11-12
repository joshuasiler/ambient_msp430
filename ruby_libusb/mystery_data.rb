# a program to dump hid datagrams from mysterious hid devices to aid in reverse
# engineering.

require 'libusb'

# put your vendor/product id here (lsusb can help you find that)
mystery_device = USB.device_matching(0x0480, 0x001)

puts "Endpoints (#{mystery_device.endpoints.size}): "
mystery_device.endpoints.each {|e| puts e.to_s}

endpoint = mystery_device.endpoints.detect { |e| 
  e.interface.hid? and 
  e.type.string_value == "USB_ENDPOINT_TYPE_INTERRUPT" and
  e.input? }
puts "\nlistening on first hid interrupt in endpoint (#{endpoint})"
endpoint.interface.detach_kernel
endpoint.interface.hid_reports
endpoint.interface.listen_for_hid_interrupts { |interface|
  while(1) 
    data = interface.next_interrupt
    puts data
  end
}



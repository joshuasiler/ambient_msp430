require 'libusb'

# We want to find a device that supports the Keyboard k key.  Then we'll listen
# on this device for events and print out a message when K is pressed 

# This creates a usage object representing the K key
k_usage = USB::Usage.find_usage_named(/Keyboard k/)

usable_interface = nil
# search through all usb devices
hid_device = USB::devices.detect { |d| 
  # get the current configuration
  config = d.current_configuration

  #get all the interfaces within that configuration
  config.interfaces.each { |i|
    #find a hid interface that supports the k key
    if(i.hid? and i.current_alternative?)
      # we must detach the device from the kernel to get its descriptor
      i.detach_kernel
      usable_interface = i if (i.all_input_usages.include?(k_usage))
    end
  }
  
  # if we've found a usable interface, stop otherwise search the next device
  usable_interface
}

raise "Could not find a suitable device" if usable_interface.nil?
puts "Using device #{usable_interface.device.product_name}"

# listen for incoming ReportData objects
usable_interface.listen_for_hid_interrupts {
  while(1)
    report_data = usable_interface.next_interrupt
    value_for_k = report_data.get_usage_value(k_usage)
    if(value_for_k.nil?)
      puts "Datagram did not contain k key.  Got these usages instead #{report_data.set_usages.inspect}"
    else 
      puts "*****K set to value #{value_for_k}*****"
    end
  end
}

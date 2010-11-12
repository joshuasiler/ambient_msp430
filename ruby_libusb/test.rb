require 'libusb'

devices = USB::devices();
devices.each {|d| 
    begin
    puts "Dirname #{d.dirname}" 
    puts "Location #{d.location}"
    puts "Filename #{d.filename}"
    puts "Device Num #{d.device_num}"
    puts "Children num #{d.children.size}" 
    puts "USB version #{d.usb_version.join(".")}"
    puts "Device version #{d.device_version.join(".")}"
    puts "Vendor id #{"0x%04x" % d.vendor_id}"
    puts "Product id #{"0x%04x" % d.product_id}"
    puts "Max packet size #{d.max_packet_size_for_endpoint_0}"
    puts "Device class #{d.device_class}"
    puts "Device subclass #{d.device_subclass}"
    puts "Device protocol #{d.device_protocol}"
    puts "Manufacturer #{d.manufacturer}"
    puts "Serial number #{d.serial_number}"
    puts "Product name #{d.product_name}"
    config = d.current_configuration
    puts "Self powered #{config.self_powered?}"
    puts "Remote wakeup #{config.remote_wakeup?}"
    puts "Max power #{config.max_power} milliamps"
    puts "Current configuration: #{config.value}"
    puts "HID #{d.hid?}"
    puts "Interfaces:\n#{d.interfaces.join("\n")}"
    puts "Endpoints:\n#{d.endpoints.join("\n")}"
    puts "==========================="
  rescue USB::UsbException => e
  end
}


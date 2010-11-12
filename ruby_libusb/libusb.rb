require 'cmacrovalue'
require 'libusb_c'
require 'json'

# A module for interacting with USB devices in a generic way.  This library
# especially includes support for discovering HID interfaces.  For some examples
# check out map_example.rb and mystery_data.rb.  Here one to get you started
# through:
#
#   require 'libusb'
#   
#   # We want to find a device that supports the Keyboard k key.  Then we'll listen
#   # on this device for events and print out a message when K is pressed 
#   
#   # This creates a usage object representing the K key
#   k_usage = USB::Usage.find_usage_named(/Keyboard k/)
#   
#   usable_interface = nil
#   # search through all usb devices
#   hid_device = USB::devices.detect { |d| 
#     # get the current configuration
#     config = d.current_configuration
#   
#     #get all the interfaces within that configuration
#     config.interfaces.each { |i|
#       #find a hid interface that supports the k key
#       if(i.hid? and i.current_alternative?)
#         # we must detach the device from the kernel to get its descriptor
#         i.detach_kernel
#         usable_interface = i if (i.all_input_usages.include?(k_usage))
#       end
#     }
#     
#     # if we've found a usable interface, stop otherwise search the next device
#     usable_interface
#   }
#   
#   raise "Could not find a suitable device" if usable_interface.nil?
#   puts "Using device #{usable_interface.device.product_name}"
#   
#   # listen for incoming ReportData objects
#   usable_interface.listen_for_hid_interrupts {
#     while(1)
#       report_data = usable_interface.next_interrupt
#       value_for_k = report_data.get_usage_value(k_usage)
#       if(value_for_k.nil?)
#         puts "Datagram did not contain k key.  Got these usages instead #{report_data.set_usages.inspect}"
#       else 
#         puts "*****K set to value #{value_for_k}*****"
#       end
#     end
#   }
#
#
# There's quite a few classes here so let me break it down a bit.
#
# *Device* represents a particular USB device, although this device might have
# several different functions (for example a combination scanner printer
# keyboard massage chair).  Use USB.devices or USB.device_matching to find these
# devices.  Devices contain information that lets you find a particular device -
# strings and numbers that represent the vendor and product.  Devices
# contain Configuration objects.
#
# *Configuration* represents a particular mode that a USB device might be in.
# In theory, a device could have several modes where it acts like something
# completely different (say a mouse mode and a scanner mode).  Slighly more
# likely is a low-power mode and a high power mode with more features.  These
# modes are always mutally exclusive and very few devices have them.  Most of
# the time you should be able to get away with just calling
# current_configuration from the Device object and ignoring the others.
# Configuration objects contain Interface objects.
#
# *Interface* represents a particular function (like a scanner, or a hub, or a
# human interface device).  Interfaces have some data that lets you find out
# what sort of feature they are (interface_class in particular) and often have
# subdescriptors that give you more information about this particular feature.
# The only type that's well supported right now is Human Interface Devices
# (HIDs).  Most hid specific functions are in the Interface class.  Interfaces
# contain Endpoint objects which are mechanisms for communicating with the USB 
# device.
#
# *Endpoint* is a class representing a channel of communication to a USB device.
# There are various types of endpoints defined in the spec but the only one well
# supported right now are Interrupt Input endpoints (of particular use for HID
# devices).
#
# *ReportDescriptor* is something USB HID devices have, and it's accessed by
# using the hid_descriptor methods in the Interface class.  A hid_descriptor
# defines several Report objects that the USB HID device might send to or
# receive from the host.
#
# *Report* is a part of a hid descriptor.  A report can have both an input and
# output component.  Reports contain functions for parsing datapackets from the
# hid device (althogh these are usually best accessed indirectly through the
# ReportData object) and also InputOutputFeature objects which contain metadata
# about each element in the Report.
#
# *InputOutputFeature* is a single set of releated fields within a report.  They
# can have metadata with regard to the size and contents of the report.  One of
# the most useful bits of metadata is Usage objects, which correspond to
# particular tiny features (like a button for the letter K, or a Battery led)
# that the device supports.  Usage meanings are defined in the HID spec.
#
# *Usage* objects represent usages and give some handy methods for parsing them.
#
# *ReportData* represents a single data packet from a USB hid device.  It has
# functions for accessing that data (parsed according to the ReportDescriptor).
# These objects are created by the Interface method listen_for_hid_interrupts
#
# *UsageListener* is a small handy class for people who want to listen for a
# partciular set of usages from a HID device.
#
# 
# Copyright 2006 Michael Hewner
module USB
  @@devices = nil

  # Get the current USB devices.  Note that this does not reload the device list
  # on subsquent calls - if you want and updated picture of USB devices you
  # should call load_devices to manually reload them
  def USB.devices
    load_devices if(@@devices.nil?) 
    return @@devices
  end
  
  # Returns a device matching the specified vendor and product id, otherwise and
  # exception is thrown
  def USB.device_matching(vendor_id, product_id)
    result = USB::devices.detect {|d| d.vendor_id == vendor_id and d.product_id == product_id}
    raise UsbException("No device matching vendor id #{vendor_id} and product_id #{product_id} found") if result.nil?
    return result
  end

  # A class representing a USB Device attached to the system.  In USB, devices
  # have 1 or more configurations but only one can be active at a particular
  # time.  To get device objects, call USB::devices which will return all
  # available devices on the system.  To find a particular device, it's usual
  # to look for a particular Device::vendor_id Device::product_id combination.
  # For human readable searching, Device::product_name and Device::manufacturer
  # might be useful.
  #
  # An example:
  # 
  #   require 'libusb'
  #
  #   devices = USB::devices();
  #   devices.each { |d|
  #     puts "Device vendor: #{d.vendor_id} product: #{d.product_id}"
  #   }
  class Device

    def Device.create_cmacrovalue(value, map_name) #:nodoc:
      CMacroValue.new(value, @@macro_maps[map_name])
    end
    
    # The device class has maps of constants that are used interally and passed
    # around as CMacroValue constants.  This lets you access these maps (most of
    # the time there should be no reason to do this though).  The
    # maps are stored as a hash of map names, containings maps betweens
    # constants and their values
    def Device.macro_maps
      return @@macro_maps
    end

    # The device class has maps of constants that are used interally and passed
    # around as CMacroValue constants.  Normally this isn't a problem but occasonally
    # it's useful to find a value for a particular constant.
    #
    # <tt>USB::Device::get_value_for("data_item_flags", "InputOutputFeature::NONVOLATILE")</tt>
    def Device.get_value_for(map_name, value_string)
      return @@macro_maps[map_name].invert[value_string]
    end

    # Returns all the configurations available for this USB device.  Normally a
    # device only has one configuration but occasonally more are possible.  Only
    # one configuration is active at one time.
    def configurations
      @configurations = load_configurations if @configurations.nil?
      return @configurations
    end
    
    # Returns the current configuration object.  Not that this requires
    # communication with the device so it can be (slightly) expensive.  The
    # current configuration stores information about the power chacteristics of the
    # device as well as interfaces to the device.
    def current_configuration
      current_configuration_val = current_configuration_value
      return configurations.detect { |c| c.value == current_configuration_val }
    end

    # Returns all interfaces available in all configurtions for this device.
    # Bear in mind that some interfaces might not be currently active, either
    # because they belong to an inactive configuration or because they are not a
    # currently active alternative interface.
    def interfaces
      interface_lists = configurations.collect {|each| each.interfaces}
      interface_lists.flatten!
      return interface_lists
    end

    # Returns all endpoints available in all configurations and interfaces.
    # Some endpoints might not be active because they are part of an inactive
    # configuration or inactive alternate interface.
    def endpoints
      endpoint_lists = interfaces.collect { |each| each.endpoints }
      endpoint_lists.flatten!
      return endpoint_lists
    end

    # Returns true if any interface on the device, in any configuration or
    # alternate interface, is a human interface device.  Note that most useful
    # human interface device related functions are within the Interface object
    def hid?
      interfaces.each {|i|
        if(i.interface_class.string_value == "USB_CLASS_HID")
          return true;
        end
      }
      return false;
    end

  end

  # All data that comes from or to a USB device passes through an endpoint.  All
  # devices have an implict Endpoint 0 Control endpoint where all the predefined
  # USB requests flow and can define a number of additional endpoints (15 input
  # and 15 ouput for full/high speed devices or 2 for low speed devices).
  #
  # Currently ruby usb only has support for interrupt in endpoints.  Hopefully
  # more endpoint support will be coming soon!
  #   
  #  mystery_device = USB.device_matching(0x046e, 0x530a)
  #  endpoint = mystery_device.endpoints.detect { |e| 
  #    e.type.string_value == "USB_ENDPOINT_TYPE_INTERRUPT" and
  #    e.input? }
  #  endpoint.interface.detach_kernel
  #  endpoint.listen_for_interrupts {
  #    while(1)
  #      puts endpoint.next_interrupt.unpack("B*")
  #    end
  #  }
  class Endpoint
    attr_reader :interface
    def to_s
      direction = ""
      direction = "input" if input?
      direction = "output" if output?
      return "Type #{type} #{direction} Number #{number} MaxPacSize #{max_packet_size} interval #{interval}"
    end

    # This is the preferred way to listen for interrupts.  It take a block which is passed
    # the endpoint.  The device is kept open for the block and then closed when
    # block ends.
    #
    # Internally, a C thread is spawned which continiously waits for interrupts
    # and puts them in a queue.  Therefore, there is no danger of interrupt
    # values being "lost" if not handled immediately.  If interrupts are not
    # removed from the queue with next_interrupt however, they will consume
    # memory.
    def listen_for_interrupts
      begin
      start_interrupt_listening
      yield(self)
      ensure
        stop_interrupt_listening
      end
    end

    # Blocks until the next interrupt is available and then returns it as a ruby
    # character string.  
    #
    # This is the preferred way to get interrupt data and should be used within
    # a listen_for_interrupts block.  It is possible to call next interrupt
    # after a listen_for_interrupts block has ended (it will return interrupts
    # that were in the queue but never processed).  Obviously though, if you
    # call next_interrupt outside of a listen_for_interrupt block and the queue
    # is empty, it will block forever.
    def next_interrupt
      while(1)
        val = next_interrupt_or_nil
        return val unless val.nil?
      end
    end

  end
  
  # Class representing the configurations available to a particular USB device.
  # Only one configuration can be active at a time.  Some USB devices let your
  # change thc current configuration programmmatically
  class Configuration
    attr_reader :device

    # Returns true if this is the currently active configuration.  If it's not,
    # don't attempt to use this configuration.  This call requires communication
    # with the device
    def current_configuration?
      return value == @device.current_configuration
    end

    # Gets all the interfaces in the configuration
    def interfaces
      @interfaces = load_interfaces if @interfaces.nil?
      return @interfaces;
    end

    def to_s
      return "<Configuration #{self_powered? ? "self powered" : "bus powered" } #{ remote_wakeup? ? "remote wakeup" : ""} max_milliamps #{max_power}>"
    end
  end
  
  # A interface contains data about a certian function or feature that a device
  # implements (like say being a HUB or a human interface device.  A particular
  # configurtion may have several different interfaces, and some might not be
  # comptable with other.  These is represented by the current_alternative
  # field, which selects between multiple interfaces with the same interface id.
  #
  # Each interface might have several endpoints that correspond to the
  # interface's function.
  class Interface
    attr_reader :device, :configuration

    # Returns true if this interface is a human interface device
    # Human interface devices have report descriptors that decide 
    # the format of their data
    def hid?
      return (interface_class.string_value == "USB_CLASS_HID")
    end
    
    def to_s
      return "Interface #{interface_string} class #{interface_class} subclass #{subclass} protocol #{protocol} alternate setting        #{alternate_setting} number #{number} is usable #{usable?}"
    end

    # returns true if the device is the currently selected alternative for
    # all devices in the configuration with the same interface id
    def current_alternative?
      return current_alternative == alternate_setting
    end

    # Returns true if this interface is in the currently active configuration
    # and it;s the current alternative
    def usable?
      return (current_alternative? and @configuration.current_configuration?)
    end

    # For a hig interface, this returns an ReportDescriptor object, which is a
    # parsed representation of the HID report that the device returns
    def hid_report_descriptor
      raise(UsbException, "Not a hid interface") unless hid?
      @report_descriptor = load_hid_report_descriptor() if @report_descriptor.nil?
      return @report_descriptor
    end

    # All the report objects from the hid_report_descriptor.  A device may have
    # serval reports, all with different report ids
    def hid_reports
      return hid_report_descriptor.reports
    end

    # This corresponds to the listen_for_interrupts function in the Endpoint
    # class.  Within the block the device is open and interrupts are being
    # recorded into a queue by a seperate thread.  You can get the next
    # interrupt by using the next_interrput function.
    #
    # This function listens on the endpoint returned by hid_interrupt_input
    def listen_for_hid_interrupts
      raise(UsbException, "Not a HID interface") unless hid?
      endpoint = hid_interrupt_input
      raise(UsbException, "Cannot find input interrupt endpoint for this HID") if(endpoint.nil?) 
      begin
        endpoint.start_interrupt_listening
        yield(self)
      ensure
        endpoint.stop_interrupt_listening
      end
    end
    
    # Returns the first interrupt input endpoint in the interface, which I
    # assume to be the correct interrupt for HID reports
    def hid_interrupt_input
      raise(UsbException, "Not a HID interface") unless hid?
      return endpoints.detect {|i| i.type.string_value == "USB_ENDPOINT_TYPE_INTERRUPT" and i.input? }
    end

    # Returns a ReportData object which represents a single data packet sent
    # from the device.  Should be called from within a listen_for_hid_interrputs
    # block
    def next_interrupt
      return ReportData.new(hid_interrupt_input.next_interrupt, hid_report_descriptor)
    end

    # All the endpoints for this interface
    def endpoints
      @endpoints = load_endpoints if @endpoints.nil?
      return @endpoints
    end

    # Returns all the usages specified within the hid interface's input reports
    def all_input_usages
      raise(UsbException, "Not a HID interface") unless hid?
      return hid_reports.collect { |r| r.inputs.collect { |i| i.all_usages }}.flatten
    end
  end
 
  # A small class that lets you easily write a program that registers interest in
  # particular usages and then gets notified when they change.  He's a program
  # that registers input in the "f" keyboard press:
  #  
  #   require 'libusb'
  #   
  #   hid_device = USB::devices.select { |d| d.hid? }[0]
  #   hid_interface = hid_device.interfaces { |i| i.hid? }[0]
  #   hid_interface.detach_kernel
  #   listener = USB::UsageListener.new(hid_interface)
  #   usage = USB::Usage.find_usage_named(/Keyboard f/)
  #   puts "listening for usage #{usage}"
  #   listener.on_usage_value_change(usage) { |new_val| puts "New F value: #{new_val}" }
  #   listener.start_listening
  class UsageListener

    # Takes the hid interface you want to listen to
    def initialize(interface)
      @interface = interface
      @usages_to_watch = Hash.new
    end

    # Here you register a listener block for a particular usage
    def on_usage_value_change(usage, &block_to_execute)
      @usages_to_watch[usage] = [nil, block_to_execute]
    end

    # Once start_listening is called (this is blocking BTW) your callbacks will
    # be called whenever an interrupt corresponding to them comes from the
    # device
    def start_listening
      @interface.listen_for_hid_interrupts { | interface |
        while(1)
          data = interface.next_interrupt
          @usages_to_watch.each_pair { |usage, command|
            usage_val = data.get_usage_value(usage)
            if(usage_val != command[0])
              command[0] = usage_val
              command[1].call(usage_val)
            end
          }
        end
      }
    end
  end

  # Represents Usages, which is the mechanism USB uses to register what kind of
  # input/output a human interface device supports
  class Usage
    def to_s
       return "#{usage_page_name} (#{usage_page})::#{usage_name} (#{usage})"
      return "(#{usage_page})::(#{usage})"
    end

    def ==(other_usage)
      return (extended_usage == other_usage.extended_usage)
    end

    # Returns a usgae matching the regex you pass, assumming one can be found in
    # usages.json
    def Usage.find_usage_named(regex)
      file = File.new("usages.json")
      string = file.readlines.join("")
      result = SafeJSON.parse(string)
      strings_list = []
      result.each_pair { |page_num, page| page.each_pair { |usage_num, usage_name| strings_list << [page_num.to_i(16), usage_num.to_i(16), "#{page["name"]}::#{usage_name}"] unless usage_num == "name"} }
      usage_nums = strings_list.detect { |each| each[2] =~ regex }
      return Usage.create_usage(usage_nums[0], usage_nums[1]);
    end
  end

  # A report descriptor represents the entire descriptor returned from a HID
  # device.  Most data is contained within the reports themselves though
  class ReportDescriptor
    # If there is only one report and it does not specify an id, data passed
    # from the usb device can be sent without an report_id prefix
    def uses_report_id?
      return false if(reports().size == 1 and reports()[0].id == 0)
      return true
    end

    # Get the Repord objects associated with this descriptor
    def reports
      @reports = load_reports if @reports.nil?
      return @reports
    end

    # Get the report with the given report_id.  This raises a USBException if
    # the report is not found
    def get_report(id)
      report = reports.detect { |r| r.id == id }
      raise(UsbException, "No report with id #{id}") if report.nil?
      return report
    end
  end

  # This class represents the Input, Output, or Feature mainitems within a USB
  # Report descriptor.  They determine the details of a report's structure and
  # content.  There is usually not a lot of need to use this class directly -
  # much happens implictly through the ReportData object
  class InputOutputFeature

    # Returns an array of pairs [Usage, value] corresponding to the binary
    # string that is passed.  Note that for array items, all possible usages
    # might not be in this resultant list
    def set_usages_for_data(binary_string)
      result = []
      i = 0
      values_for_data(binary_string).each { |value|
        if(array?)
          result << [usage_for_field(value), 1] if value > 0
        else
          result << [usage_for_field(i), value]
        end
        i = i + 1
      }
     return result
    end

    # Returns an array of numbers corresponding to the integer values of each
    # seperate segment of the input.  Negative integers are not currently
    # supported (yet)
    def values_for_data(binary_string)
      return (0...report_count).collect { |i| binary_string[i*report_size, report_size].reverse.to_i(2) }
    end
    
    # Returns true if the device has one of the following characteriscs (passed
    # as a string:
    #   InputOutputFeature::DATA, InputOutputFeature::CONSTANT,
    #   InputOutputFeature::ARRAY, InputOutputFeature::VARIABLE, 
    #   InputOutputFeature::ABSOLUTE, InputOutputFeature::RELATIVE, 
    #   InputOutputFeature::NO_WRAP, InputOutputFeature::WRAP,
    #   InputOutputFeature::LINEAR, InputOutputFeature::NONLINEAR,
    #   InputOutputFeature::PREFERRED_STATE, InputOutputFeature::NO_PREFERRED_STAT,
    #   InputOutputFeature::NULL_STATE, InputOutputFeature::NO_NULL_STATE,
    #   InputOutputFeature::NONVOLATILE, InputOutputFeature::VOLATILE,
    #   InputOutputFeature::BIT_FIELD, InputOutputFeature::BUFFERED_BYTES,
    def flag_set?(flag_string)
      return flag_set_internal?(USB::Device::get_value_for("data_item_flags", flag_string))
    end

    # Returns true if the data item is a array item. false if it is a variable
    # item
    def array?
      return flag_set?("InputOutputFeature::ARRAY")
    end
  end

  # Represents a single data report sent from a device to the host
  class ReportData

    # Takes a character string of input data and the report descriptor
    # corresponding to the device that created it
    def initialize(data, descriptor)
      @binary_string = data.reverse.unpack("B*")[0].reverse;
      @descriptor = descriptor
    end

    # True if this data packet was prefixed with a report_id
    def report_id?
      return @descriptor.uses_report_id?
    end

    # Report_id for this packet
    def report_id
      raise UsbException, "Attempt to get report_id for a packet that did not have a report_id" unless report_id?
      return @binary_string[0,8].reverse.to_i(2)
    end

    # Retruns a binary string corresponding to the data portion of the packet
    # (without the report_id)
    def data_part
      if(report_id?)
        return @binary_string[8..@binary_string.size]
      else
        return @binary_string
      end
    end

    # The report within the report descriptor that describes this data's
    # formatting
    def report
      if(report_id?)
        @descriptor.get_report(report_id)
      end
      return @descriptor.reports[0]
    end

    # Returns a list of pairs [Usage, value] containing all the usages that are
    # currently set in this report and thier values.  Note that this list
    # contents can vary, based on the contents of array items in the report
    def set_usages
      index = 0
      result = []
      report.inputs.each {|input|
        result |= input.set_usages_for_data(data_part[index, input.length])
        index = index + input.length
      }
      return result
    end

    #Get the value of a particular usage, or return nil if it is not set in this
    #data packet
    def get_usage_value(usage)
      set_usages.each { |i|
        return i[1] if i[0] == usage
      }
      return nil
    end

    # Return an array corresponding to the integral values of the data retruned.
    # This can be useful for undertsanding a device's format
    def values
      index = 0
      result = []
      report.inputs.each {|input|
        result |= input.values_for_data(data_part[index, input.length])
        index = index + input.length
      }
      return result
    end

    def to_s
      return "Report #{report_id? ? report_id : ""} data: [#{values.join(", ")}]"
    end
  end
 end
USB::load_devices

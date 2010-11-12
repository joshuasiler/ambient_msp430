require 'libusb'
require 'breakpoint'

def remap_hid(index)
  puts "remapping #{index}"
  interface = $hids[index].interfaces[0]
  interface.detach_kernel
  # puts interface.hid_report_descriptor
  puts interface.hid_reports[0].inputs[0].all_usages.join(", ")
  interface.listen_for_hid_interrupts {
    while(1)
      puts interface.next_interrupt.inspect
    end
  }
end

$hids = USB::devices.select { |d| d.hid? }
puts "$hids \n===================="
$hids.each_index { |i| puts "#{i}. #{$hids[i].product_name}"}
e1 = remap_hid(0)

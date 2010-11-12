# Build a SAFE JSON parser
# and execute JSON parsing safely

module SafeJSON
  require 'monitor'
  
  def SafeJSON.build_safe_json
    ret = nil
    waiter = ''
    waiter.extend(MonitorMixin)
    wait_cond = waiter.new_cond
    
    Thread.start do
      $SAFE = 4
      ret = Proc.new {|json|
        eval(json.gsub(/(["'])\s*:\s*(['"0-9tfn\[{])/){"#{$1}=>#{$2}"})
      }
      waiter.synchronize do
        wait_cond.signal
      end
    end
    
    waiter.synchronize do
      wait_cond.wait_while { ret.nil? }
    end
    
    return ret
  end
  
  @@parser = SafeJSON.build_safe_json
  
  # Safely parse the JSON input
  def SafeJSON.parse(input)
    @@parser.call(input)
  rescue SecurityError
    return nil
  end
  
end

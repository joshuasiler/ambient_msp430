# This class is for values that are normally represented as C #define macros.
# So objects of this class have an integer value as well as a string associated
# with them (the name of the #define in C).  These macrovalues are linked to
# maps in a particular class so you can always see the possible C Macro values
# in ruby if you need to
class CMacroValue

  attr_reader :value, :macro_map
  
  def initialize(value, macro_map)
    @value = value;
    @macro_map = macro_map
  end

  # Get the string corresponding to the macro.  I prefer to use this for
  # comparisons instead of the integer value because it's more intention
  # revealing.  Obviously slower too though
  def string_value
    return @macro_map[@value] if @macro_map.include?(@value)
    puts @macro_map.inspect
    return "Unknown"
  end

  def to_s
    return "#{string_value} (#{value})"
  end
end

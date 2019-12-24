------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;


package common_types is
  subtype slv_8 is std_logic_vector(7 downto 0);
  subtype slv_16 is std_logic_vector(15 downto 0);
  subtype slv_24 is std_logic_vector(23 downto 0);
  subtype slv_32 is std_logic_vector(31 downto 0);
  subtype slv_64 is std_logic_vector(63 downto 0);

  type slv64_array is array (natural range <>) of slv_64;-- vivado cannot simulate the unconstrained vector yet :(    
  type slv32_array is array (natural range <>) of slv_32;-- vivado cannot simulate the unconstrained vector yet :(    
  type slv24_array is array (natural range <>) of slv_24;-- vivado cannot simulate the unconstrained vector yet :(    
  type slv16_array is array (natural range <>) of slv_16;-- vivado cannot simulate the unconstrained vector yet :(    
  type slv8_array is array (natural range <>) of slv_8;-- vivado cannot simulate the unconstrained vector yet :(

end package;

----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date:    16:27:23 02/28/2019 
-- Design Name: 
-- Module Name:    lsi_sim - Behavioral 
-- Project Name: 
-- Target Devices: 
-- Tool versions: 
-- Description: 
--
-- Dependencies: 
--
-- Revision: 
-- Revision 0.01 - File Created
-- Additional Comments: 
--
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
--use IEEE.NUMERIC_STD.ALL;

-- Uncomment the following library declaration if instantiating
-- any Xilinx primitives in this code.
--library UNISIM;
--use UNISIM.VComponents.all;


entity ezusb_lsi is
port (
  clk : in std_logic;
  reset_in: in std_logic;
  reset : out std_logic;
  
  data_clk: in std_logic;-- data sent on both edges, LSB transmitted first
  mosi : in std_logic;
  miso : out std_logic;
  stop : in std_logic;
  -- interface
  in_addr: out std_logic_vector (7 downto 0);-- input address
  in_data: out std_logic_vector (31 downto 0); -- input data
  in_strobe: out std_logic;-- 1 indicates new data received (1 for one cycle)
  in_valid:  out std_logic;-- 1 if date is valid
  out_addr: out std_logic_vector (7 downto 0);-- output address
  out_data: in std_logic_vector (31 downto 0); --output data
  out_strobe: out std_logic -- 1 indicates new data request (1 for one cycle)
);
end ezusb_lsi;

architecture Behavioral of ezusb_lsi is

begin

miso <= '0';
out_addr <= (others => '0');
out_strobe <= '0';

process
begin

in_data <= X"00000000";
in_addr <= X"00";
in_strobe <= '0';

wait until falling_edge(reset_in);
wait until rising_edge(clk);

in_data <= X"104040FF";
in_addr <= X"04";
in_strobe <= '1';

wait until rising_edge(clk);
in_strobe <= '0';
wait;
end process;


end Behavioral;


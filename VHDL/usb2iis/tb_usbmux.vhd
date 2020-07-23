----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 12/08/2018 11:58:09 PM
-- Design Name: 
-- Module Name: fifohd2axis_sim - Behavioral
-- Project Name: 
-- Target Devices: 
-- Tool Versions: 
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
use IEEE.NUMERIC_STD.ALL;
use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

use work.simtools.all;

entity fifohd2axis_sim is
--  Port ( );
end fifohd2axis_sim;

architecture Behavioral of fifohd2axis_sim is

  signal clk, reset, request, grant, valid , full, ready: std_logic;

  signal data : std_logic_vector(15 downto 0);

begin
  
  inst: entity work.s_axis_to_w_fifo
  generic map( DATA_WIDTH => 16 )
  port map(
    clk => clk,
    reset => reset,
    tx_req => request,
    tx_grant => grant,
    tx_full => full,
    s_axis_tvalid => valid,
    s_axis_tready => ready,
    s_axis_tdata => data
  );



  clk_gen( clk, 48_000_000.0);
  signal_gen( reset, clk, '1', 4, '0', 0, '0', 0, false);
  signal_gen( valid, clk, '0', 6, '1', 30, '0', 0, false);

process(clk)
  variable temp :std_logic_vector(1 downto 0);
begin
  if rising_edge(clk) then
    temp := temp(0) & request;
    grant <= 
    --  '1';
    not reset and request ;
    --temp(1) and temp (0);
  end if;
end process;

process(clk)
  variable count :std_logic_vector(15 downto 0);
begin
  if rising_edge(clk) then
    if reset = '1' then
      count := ( others => '0');
    elsif ready = '1' and valid = '1' then
      count := count + 1;
    end if;
  end if;
  data <= count;
end process;


process(clk)
variable count :std_logic_vector(2 downto 0);
begin
  if rising_edge(clk) then
    if reset = '1' then
      count := ( others => '0');
    else
      count := count + 1;
    end if;
  end if;
  if (data < 11 and count = 0) or 
     (data > 11 and (count = 0 or count = 1 )) then
    full <= '1';
  else
    full <= '0';
  end if;
end process;

end Behavioral;

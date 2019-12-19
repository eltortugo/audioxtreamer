library IEEE;
  use IEEE.STD_LOGIC_1164.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
  use IEEE.NUMERIC_STD.all;
  use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

  use work.simtools.all;
  use work.common_types.all;

entity tb_samplingratedetect is
  --  Port ( );
end tb_samplingratedetect;

architecture Behavioral of tb_samplingratedetect is


  signal usb_clk  : std_logic;
  signal reset: std_logic;

  signal yamaha_clk: std_logic;

  signal count_start : std_logic := '0';
  signal done_counting : std_logic := '0';

  signal sample_counter : slv_16 := (others => '0');

begin

  clk_gen(usb_clk, 48_000_000.0 );
  clk_gen(yamaha_clk, 11_289_600.0 ); --44.1 khz
  --clk_gen(yamaha_clk, 24_576_000.0 ); --96 khz

  process
  begin 
    reset <= '1';
    wait for 100 ns;
    reset <= '0';
    wait;
  end process;
 


uut : entity work.sampling_rate_detect
  generic map ( gate_length => 2400000 )
  port map (
    rst       => reset,
	 clk       => usb_clk,
    pcm_clk  => yamaha_clk,
    sr_count  => sample_counter
  );

end architecture;

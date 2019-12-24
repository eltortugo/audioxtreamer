------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.common_types.all;
  use work.simtools.all;


entity tb_midi_io is
end entity;

architecture rtl of tb_midi_io is

  signal clk  : std_logic;
  signal reset: std_logic;
  
  signal serial: std_logic;
  signal rx_data: slv_8;
  signal rx_valid: std_logic;


  signal tx_data: slv_8;
  signal tx_valid: std_logic;
  signal tx_ready: std_logic;

begin

  process
  begin 
    reset <= '1';
    wait for 100 ns;
    reset <= '0';
    wait;
  end process;

  clk_gen(clk, 48_000_000.0 );

  p_sim : process

  begin
    tx_data <= X"CD";
    wait until falling_edge(reset);

    if tx_ready = '0' then
      wait until tx_ready = '1';
    end if;
    tx_data <= X"A5";
	 
    wait until rx_valid = '1';
    assert rx_data = tx_data;
    wait;
  end process;
  
  tx_valid <= tx_ready;

  midi_io_inst: entity work.midi_io
    generic map (
      NR_CHANNELS => 1
    )
    port map (
      clk         => clk,
      reset       => reset,
      rx(1)       => serial,
      rx_data(1)  => rx_data,
      rx_valid(1) => rx_valid,
      rx_ready(1) => '1',
      tx(1)       => serial,
      tx_data(1)  => tx_data,
      tx_ready(1) => tx_ready,
      tx_valid(1) => tx_valid
    );

  
  
end architecture;

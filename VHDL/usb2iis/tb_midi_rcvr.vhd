library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.simtools.all;

entity tb_midi_rcvr is
end entity;

architecture rtl of tb_midi_rcvr is
  signal usb_clk  : std_logic;
  signal reset: std_logic;
  signal usb_data : std_logic_vector(15 downto 0);
  
  signal usb_rd_req: std_logic;
  signal wr_counter : natural range 0 to 15;

  type payload is array (natural range <>) of std_logic_vector(7 downto 0);
  constant test_data: payload := 
  ( X"96", X"69",
    X"69", X"96",
    X"ff", X"ff",
    X"A1", X"B1",
    X"A2", X"B2",
    X"A3", X"B3",
    X"A4", X"B4",
    X"A5", X"B5",
    X"A6", X"B6",
    X"A7", X"B7",
    X"A8", X"B8",
    X"A9", X"B9",
    X"AA", X"BA",
    X"AB", X"BB",
    X"AC", X"BC"
  );

begin
  
  clk_gen(usb_clk, 48_000_000.0 );
  
  process
  begin 
    reset <= '1';
    wait for 100 ns;
    reset <= '0';
    wait;
  end process;
  
  process (usb_clk)
  begin
    if rising_edge(usb_clk) then
      if reset = '1' then
        usb_data <= X"EFEF";
        wr_counter <= 0;
      else
        if usb_rd_req = '0' and wr_counter < 14 then
          wr_counter <= wr_counter+1;
        end if;
        usb_data <= test_data((wr_counter*2)+1)& test_data(wr_counter*2); -- simulate the flop clock/transfer delay
      end if;
    end if;
  end process;
  
  cy16_to_fifo_inst: entity work.cy16_to_fifo
    generic map (
      max_sdo_lines => 9
    )
    port map (
      usb_clk       => usb_clk,
      reset         => reset,
      usb_data      => usb_data,
      usb_oe        => '1',
      usb_empty     => '0',
      usb_rd_req    => usb_rd_req,
      nr_outputs    => x"b",
      out_fifo_full => '0',
      out_fifo_wr   => open,
      out_fifo_data => open,
      midi_out_wr   => open,
      midi_out_data => open
    );

end architecture;
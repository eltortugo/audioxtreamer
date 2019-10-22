library IEEE;
  use IEEE.std_logic_1164.all;

  use ieee.std_logic_unsigned.all;
  use ieee.std_logic_arith.all;

  use work.common_types.all;

entity pcmio is
  generic (
    sdo_lines: positive := 32;
    sdi_lines: positive := 32
  );
  port (
    f256_clk: in  std_logic;
    sd_reset: in  std_logic;
    word_clk: in  std_logic;

    spdif_out1: out std_logic;

    out_fifo_rd_data: in slv24_array(0 to (sdo_lines*2)-1);
    out_empty : in std_logic;
    out_fifo_rd_en : out std_logic;

    in_fifo_wr_data: out slv24_array(0 to (sdi_lines*2)-1);
    in_fifo_wr_en : out std_logic;

    sd_in : in std_logic_vector(sdi_lines-1 downto 0);
    sd_out : out std_logic_vector(sdo_lines-1 downto 0)
  );
end entity;

architecture rtl of pcmio is

  constant bit_depth   : positive := 24;

  signal sdout_shifter: slv24_array(0 to sdo_lines-1);
  signal sdout_load_shifter : std_logic;
  signal sdout_is_padding : std_logic;
  signal word_clk_reg1: std_logic;
  signal word_clk_reg2: std_logic;
  signal fifo_wr_data: slv24_array(0 to (sdi_lines*2)-1);

  signal sd_counter : std_logic_vector (7 downto 0) := X"00";
  signal sd_record_only: std_logic;
  signal input_shift_enable: std_logic;
  
  attribute keep : string;
  attribute keep of word_clk : signal is "true";

begin

  ------------------------------------------------------------------------------------------------------------
  -- n256 counter triggered by falling edge of the input word clock.
  fs256_counter : process ( f256_clk )
  begin
    if rising_edge (f256_clk) then

      word_clk_reg1 <= word_clk; -- sync LR
      word_clk_reg2 <= word_clk_reg1;

      if sd_reset = '1' then 
        sd_counter <= X"00";
      elsif word_clk_reg2 = '1' and word_clk_reg1 = '0' then -- falling edge!!! start of the cycle
        sd_counter <= X"02"; -- because this is actually 2 behind.
      else
        sd_counter <= sd_counter + 1;
      end if;
    end if;
  end process;

  ------------------------------------------------------------------------------------------------------------
  --SPDIF out
  --spdif_tx_inst: entity work.spdif_tx
  --  port map (
  --    bit_clock => f256_clk,
  --    cke       => sd_counter(0),
  --    bit_counter => sd_counter(6 downto 1),
  --    l_ch      => out_fifo_rd_data(0),
  --    r_ch      => out_fifo_rd_data(1),
  --    spdif_out => spdif_out1
  --  );
spdif_out1 <= '1';
  ------------------------------------------------------------------------------------------------------------
  --TBI: add a port to select the padding scheme between left/right/i2s etc

  --sdout_is_padding <= '1' when sd_counter(6 downto 2) = 0 or sd_counter(6 downto 2) > bit_depth else '0'; --skip the first bit then msb to lsb and pad the rest
  --sdout_is_padding <= '1' when sd_counter(6 downto 5) = 0 else '0'; --pad the first and then msb to lsb (right justified)
  sdout_is_padding <= '0'; --left justified
  --sdout_is_padding <= not sd_counter(6); --pad the first and then msb to lsb (right justified) just the high 16 bits

  ----------------------------------------------------------------------------------------------------------
  process( f256_clk )
  begin
    if rising_edge(f256_clk) then

      if sd_reset = '1' or  sd_counter /= X"FD" then
        out_fifo_rd_en <= '0';
      else
        out_fifo_rd_en <= '1';
      end if;

      --delay the load shifter by one cycle to compensate the fifo putting the actual data on the bus
      if sd_counter(6 downto 0) = "1111110" and out_empty = '0'  then
        sdout_load_shifter <= '1';
      else
        sdout_load_shifter <= '0';
      end if;

    end if;
  end process;

  ----------------------------------------------------------------------------------------------------------
  output_shifter : for i in 0 to sdo_lines-1 generate
    -- load a 24 shift register and clock out
    sd_out(i) <= '0' when sdout_is_padding = '1' else sdout_shifter(i)(23);
    process( f256_clk )
    begin
      if rising_edge(f256_clk) then
        if sd_reset = '1' then 
          sdout_shifter(i) <= (others =>'0');
        elsif sdout_load_shifter = '1' then
          if sd_counter(7) = '0' then --we are loading the shifter one clock before the start of L , when LR is 1
            sdout_shifter(i) <= out_fifo_rd_data(i*2);
          else
            sdout_shifter(i) <= out_fifo_rd_data((i*2)+1);
          end if;
        elsif sdout_is_padding = '0' and sd_counter(1 downto 0) = "11" then
          sdout_shifter(i) <= sdout_shifter(i)(bit_depth -2 downto 0) & '0';
        end if;
      end if;
    end process;
  end generate;

  ------------------------------------------------------------------------------------------------------------
  --Inputs to fifo


  sd_record_only <= '0';-- we will have at least one active output

  -- msb left(justified)
  -- the input shows up one clock after the word clock transition, so sample in the middle of the window (3)
  input_shift_enable <= '1' when sd_counter(6 downto 2) < bit_depth and sd_counter(1 downto 0) = "11" else '0';
  
  in_fifo_wr_en <= '1' when sd_counter = X"FE" else '0';

  input_shifter : for i in 0 to sdi_lines-1 generate
    process(f256_clk)
    begin
      if rising_edge(f256_clk) then
        if sd_reset = '0' and input_shift_enable = '1' then
          if sd_counter(7) = '1' then
            fifo_wr_data(i*2) <= fifo_wr_data(i*2)(bit_depth-2 downto 0) & sd_in(i);
          else
            fifo_wr_data(i*2+1) <= fifo_wr_data(i*2+1)(bit_depth-2 downto 0) & sd_in(i);
          end if;
        end if;
      end if;
    end process;
  end generate;
  ------------------------------------------------------------------------------------------------------------

  in_fifo_wr_data <= fifo_wr_data;

end architecture;

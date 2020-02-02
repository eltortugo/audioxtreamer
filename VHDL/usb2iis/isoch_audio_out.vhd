------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;
use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;


use work.common_types.all;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
entity isoch_audio_out is
generic(
  max_sdo_lines: positive := 32
);
  Port (
    --usb interface
    usb_clk : in STD_LOGIC;
    reset : in STD_LOGIC;

    s_axis_tdata : in slv_16;
    s_axis_tvalid  : in std_logic;
    s_axis_tready : out STD_LOGIC;

    nr_outputs : in std_logic_vector(7 downto 0);

    out_fifo_full : in std_logic;
    out_fifo_wr   : out std_logic;
    out_fifo_data : out slv24_array(0 to (max_sdo_lines*2)-1);

    sof           : in std_logic

    );
end entity;

------------------------------------------------------------------------------------------------------------

architecture rtl of isoch_audio_out is

-- RX FSM
type rx_state_t is (init, w0, w1, w2);
signal rx_state : rx_state_t;
--attribute fsm_encoding : string;
--attribute fsm_encoding of rx_state : signal is "one-hot";

signal outs_counter, active_outs: natural range 0 to max_sdo_lines-1;
signal eof        : std_logic;
signal eof_r      : slv_16;

-- out_fifo signals

signal out_fifo_data_regs : slv24_array(0 to (max_sdo_lines*2)-1);

------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
begin

------------------------------------------------------------------------------------------------------------
rcvr_FSM: process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' or sof = '1' then
      eof <= '0';
      eof_r <= x"0000";
    elsif s_axis_tvalid = '1' and eof = '0' then
      eof_r <= s_axis_tdata;
      if eof_r = x"55aa" and s_axis_tdata = x"aa55" then
        eof <= '1';
      end if;
    end if;

    if reset = '1' or eof = '1' then
      rx_state <= init;
    else
      case rx_state is
      when init =>
        if active_outs /= 0 then
          rx_state <= w0;
        end if;
      when w0 =>
        if s_axis_tvalid = '1' then
          rx_state <= w1;
        end if;
      when w1 =>
        if s_axis_tvalid = '1' then
          rx_state <= w2;
        end if;
      when w2 =>
        --only if we manage to write, otherwise wait here
        if s_axis_tvalid = '1' and ( outs_counter < active_outs-1 or out_fifo_full = '0') then
          rx_state <= w0;
        end if;
      end case;
    end if;
  end if; 
end process;

------------------------------------------------------------------------------------------------------------
active_outs <= to_integer(unsigned(nr_outputs))/2;
------------------------------------------------------------------------------------------------------------

s_axis_tready <= '0' when (rx_state = init and eof = '0') or (rx_state = w2 and outs_counter = active_outs-1 and out_fifo_full = '1') else '1';

------------------------------------------------------------------------------------------------------------

process (usb_clk)
begin
  if rising_edge (usb_clk) then
    if reset = '1' or (sof = '1' and eof = '1') then
      outs_counter <= 0;
    elsif rx_state = w2 and s_axis_tvalid = '1' then
      if outs_counter < active_outs-1 then
        outs_counter <= outs_counter+1;
      elsif out_fifo_full = '0' then
        outs_counter <= 0;
      end if;
    end if;
  end if;
end process;

------------------------------------------------------------------------------------------------------------

process (usb_clk)
begin
  if rising_edge (usb_clk) then
    if reset = '1' or eof = '1' then
      out_fifo_wr <= '0';
    elsif rx_state = w2 and s_axis_tvalid = '1' and outs_counter = active_outs-1 and out_fifo_full = '0' then
      out_fifo_wr <= '1';
    else
      out_fifo_wr <= '0';
    end if;
  end if;
end process;
--out_fifo_wr <= '1' when 
--  rx_state = w2 and
--  s_axis_tvalid = '1' and
--  outs_counter = active_outs-1 and
--  out_fifo_full = '0'
--  else '0';
------------------------------------------------------------------------------------------------------------
rx_fifos : for i in 0 to max_sdo_lines-1 generate

  process (usb_clk)
  begin
    if rising_edge(usb_clk) then
      if reset = '1' or eof = '1'then
        out_fifo_data_regs(i*2)     <= (others => '0');
        out_fifo_data_regs((i*2)+1) <= (others => '0');
      elsif i = outs_counter and s_axis_tvalid = '1' then
        case rx_state is
        when w0 => out_fifo_data_regs(i*2)(15 downto 0) <= s_axis_tdata;
        when w1 => out_fifo_data_regs(i*2)(23 downto 16) <= s_axis_tdata(7 downto 0);
                        out_fifo_data_regs((i*2)+1)(7 downto 0) <= s_axis_tdata(15 downto 8);
        when w2 => out_fifo_data_regs((i*2)+1)(23 downto 8) <= s_axis_tdata;
        when others =>
        end case;
      end if;
    end if;
  end process;
  out_fifo_data(i*2) <= out_fifo_data_regs(i*2);--right channels
  out_fifo_data((i*2)+1) <= out_fifo_data_regs((i*2)+1);

end generate;

end rtl;

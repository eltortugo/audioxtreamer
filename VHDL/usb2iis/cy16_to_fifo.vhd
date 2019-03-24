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
entity cy16_to_fifo is
generic(
  max_sdo_lines: positive := 32
);
  Port (
    --usb interface
    usb_clk : in STD_LOGIC;
    reset : in STD_LOGIC;
    -- DATA IO
    usb_oe  : in std_logic; --signals a read grant
    usb_data : in slv_16;
    -- FIFO control
    usb_empty : in  STD_LOGIC;
    usb_rd_req : out STD_LOGIC; --signals a read request 

    nr_outputs : in std_logic_vector(3 downto 0);

    out_fifo_full : in std_logic;
    out_fifo_wr   : out std_logic;
    out_fifo_data : out slv24_array(0 to (max_sdo_lines*2)-1);

    valid_packet : out std_logic
    );
end cy16_to_fifo;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------

architecture rtl of cy16_to_fifo is

signal rd_data : std_logic_vector(15 downto 0);

signal rd_req :std_logic;
signal stall_rd :std_logic;

signal rvalid   : std_logic;
signal final_rvalid   : std_logic;

-- RX FSM
type rx_state_t is ( rxs_idle , rxs_cmd, rxs_rd1, rxs_rd2, rxs_rd3 );
signal rx_state : rx_state_t;
attribute fsm_encoding : string;
attribute fsm_encoding of rx_state : signal is "one-hot"; 

-- packet decoding

constant header : slv_16 := X"55AA";
signal cmd_reg : slv_16;
signal cmd_valid: std_logic;

signal outs_counter, active_outs: natural range 0 to max_sdo_lines;

-- out_fifo signals

signal out_fifo_wr_regs : slv24_array(0 to (max_sdo_lines*2)-1);


------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
begin

------------------------------------------------------------------------------------------------------------
register_inputs : process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' then
      rd_data <= X"CDCD";
    else
      rd_data <= usb_data;
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
rvalid_reg : process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' then
      rvalid <= '0';
    elsif usb_empty = '0' and rd_req = '0' and usb_oe = '1' then 
      rvalid <= '1';
    else
      rvalid <= '0';
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
read_reg : process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1'
     or (outs_counter = active_outs-1 and out_fifo_full = '1') then
      stall_rd <= '1';
    else
      stall_rd <= '0';
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
rd_req <= '1' when (rx_state = rxs_rd3 and stall_rd = '1') or rx_state = rxs_idle else '0';
usb_rd_req <= rd_req;
------------------------------------------------------------------------------------------------------------
ftdi_state_FSM: process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' then
      rx_state <= rxs_idle;
    else  
      case rx_state is
      when rxs_idle =>
        if usb_oe = '1' and usb_empty = '0' then
          rx_state <= rxs_cmd;
        end if;
      when rxs_cmd =>
        if cmd_valid = '1' then
          if active_outs /= 0 then --there is output data and
            rx_state <= rxs_rd1;
          else
            rx_state <= rxs_idle;
          end if;
        end if;
      when rxs_rd1 =>
        if rvalid = '1' then
          rx_state <= rxs_rd2;
        end if;
      when rxs_rd2 =>
        if rvalid = '1' then
          rx_state <= rxs_rd3;
        end if;
      when rxs_rd3 =>
        if outs_counter < active_outs-1  then
          if rvalid = '1' then
            rx_state <= rxs_rd1;
          end if;
        else
          if out_fifo_full = '0' then --only if we manage to write, otherwise wait here
            if usb_oe = '1' and usb_empty = '0' then -- more to read?
              rx_state <= rxs_cmd;
            else
              rx_state <= rxs_idle;
            end if;
          end if;
        end if;
      end case;
    end if;
  end if; 
end process;

------------------------------------------------------------------------------------------------------------
cmd_valid <= '1' when rx_state = rxs_cmd and cmd_reg = header and rd_data = X"AA55" else '0';
valid_packet <= cmd_valid;
------------------------------------------------------------------------------------------------------------
process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' or rx_state = rxs_idle then
      cmd_reg <= X"0000";
    elsif rx_state = rxs_cmd then
      if rvalid = '1' then
        if cmd_valid = '0' then
          cmd_reg <= rd_data;
        else
          cmd_reg <= X"0000" ; -- invalidate the header
        end if;
      end if;
    end if; 
  end if;
end process;

------------------------------------------------------------------------------------------------------------
active_outs <= to_integer(unsigned(nr_outputs)) + 1 ;
------------------------------------------------------------------------------------------------------------
process (usb_clk)
begin
  if rising_edge (usb_clk) then
    if reset = '1' or cmd_valid = '1' then
      outs_counter <= 0;
    elsif rx_state = rxs_rd3 and rvalid = '1' and outs_counter < active_outs-1 then
      outs_counter <= outs_counter+1;
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
process (usb_clk)
begin
  if rising_edge (usb_clk) then
    if final_rvalid = '0' then 
      if rx_state = rxs_rd3 and rvalid = '1' then 
        final_rvalid <= '1';
      end if;
    else
      if rx_state /= rxs_rd3 then
        final_rvalid <= '0';
      end if;
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
out_fifo_wr <= '1' when 
  rx_state = rxs_rd3 and
  (rvalid = '1' or final_rvalid = '1' ) and
  outs_counter = active_outs-1 and
  out_fifo_full = '0'
  else '0';
------------------------------------------------------------------------------------------------------------
rx_fifos : for i in 0 to max_sdo_lines-1 generate

  process (usb_clk)
  begin
    if rising_edge(usb_clk) then
      if reset = '1' then
        out_fifo_wr_regs(i*2)     <= (others => '0');
        out_fifo_wr_regs((i*2)+1) <= (others => '0');
      elsif i = outs_counter and rvalid = '1' then
        case rx_state is
        when rxs_rd1 => out_fifo_wr_regs(i*2)(15 downto 0) <= rd_data;
        when rxs_rd2 => out_fifo_wr_regs(i*2)(23 downto 16) <= rd_data(7 downto 0);
                        out_fifo_wr_regs((i*2)+1)(7 downto 0) <= rd_data(15 downto 8);
        when rxs_rd3 => out_fifo_wr_regs((i*2)+1)(23 downto 8) <= rd_data;
        when others =>
        end case;
      end if;
    end if;
  end process;
  out_fifo_data(i*2) <= out_fifo_wr_regs(i*2);--right channels
  out_fifo_data((i*2)+1) <= out_fifo_wr_regs((i*2)+1);

  end generate;

end rtl;

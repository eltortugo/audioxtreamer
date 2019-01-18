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
entity usbf_to_fifo is
generic(
  max_sdo_lines: positive := 32
);
  Port (    
    --usb interface
    ft_clk : in STD_LOGIC;
    ft_reset : in STD_LOGIC;
    -- DATA IO
    ft_oe  : in std_logic; --signals a read grant
    rx_data : in slv_16;
    -- FIFO control
    ft_rxfe : in  STD_LOGIC;
    ft_rd : buffer STD_LOGIC; --signals a read request 

    params1 : out slv_16;
	 params2 : out slv_16;
    
    out_fifo_wr_full : in std_logic_vector;
    out_fifo_wr_en : out std_logic;
    out_fifo_wr_data : out slv24_array(0 to (max_sdo_lines*2)-1)
    );
end usbf_to_fifo;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------

architecture Behavioral of usbf_to_fifo is

-- FTDI interface 
signal ftdi_reset:std_logic;
signal ftdi_clk : std_logic;

signal ftdi_rd_data : std_logic_vector(15 downto 0); --registered data from ftdi

signal stall_rd :std_logic;

signal rvalid   : std_logic;
signal final_rvalid   : std_logic;

-- RX FSM
type rx_state_t is ( fts_idle , fts_cmd, fts_rd1, fts_rd2, fts_rd3 );
signal ftdi_state : rx_state_t;
-- packet decoding
constant header_size : positive := 8;
type command_words is array (2 downto 0) of std_logic_vector(15 downto 0); -- the last word will be read directly from the input
constant header : std_logic_vector(15 downto 0) := X"55AA"; --zeros are the sd_lines count bytes
signal cmd_words : command_words;
signal cmd_valid: std_logic;

signal outs_counter, active_outs: natural range 0 to max_sdo_lines;

-- out_fifo signals

signal out_fifo_wr_regs : slv24_array(0 to (max_sdo_lines*2)-1);


-- logic analyzer
attribute mark_debug : string;
attribute keep : string;

--attribute mark_debug of ftdi_rxd     : signal is "true";
--attribute mark_debug of ftdi_rxfe     : signal is "true";
--attribute mark_debug of ftdi_oe     : signal is "true";

--attribute mark_debug of ftdi_rd_data: signal is "true";
--attribute mark_debug of rvalid      : signal is "true";
--attribute mark_debug of final_rvalid: signal is "true";
--attribute mark_debug of packet_seq  : signal is "true";
--attribute mark_debug of sequence_error  : signal is "true";

attribute mark_debug of outs_counter   : signal is "true";
--attribute mark_debug of nr_outs   : signal is "true";
--attribute mark_debug of cmd_words   : signal is "true";
attribute mark_debug of ftdi_state  : signal is "true";
attribute mark_debug of cmd_valid   : signal is "true";
--attribute mark_debug of out_fifo_wr_full: signal is "true";
attribute mark_debug of out_fifo_wr_en  : signal is "true";
--attribute mark_debug of out_fifo_wr_regs: signal is "true";
--attribute mark_debug of out_fifo_wr_msb : signal is "true";

------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
begin

ftdi_clk <= ft_clk;
ftdi_reset <= ft_reset;

------------------------------------------------------------------------------------------------------------
register_inputs : process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then
    if ftdi_reset = '1' then
      ftdi_rd_data <= X"CDCD";      
    else      
      ftdi_rd_data <= rx_data;      
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
rvalid_reg : process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then
    if ftdi_reset = '1' then
      rvalid <= '0';
    elsif ft_rxfe = '0' and ft_rd = '1' and ft_oe = '1' then 
      rvalid <= '1';
    else
      rvalid <= '0';
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
read_reg : process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then
    if ftdi_reset = '1'
     or (outs_counter = active_outs-1 and out_fifo_wr_full /= 0) then     
      stall_rd <= '1';
    else
      stall_rd <= '0';
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
ft_rd <= '0' when (ftdi_state = fts_rd3 and stall_rd = '1') or ftdi_state = fts_idle else '1';
------------------------------------------------------------------------------------------------------------
ftdi_state_FSM: process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then
    if ftdi_reset = '1' then
      ftdi_state <= fts_idle;
    else  
      case ftdi_state is
      when fts_idle =>
        if ft_oe = '1' and ft_rxfe = '0' then 
          ftdi_state <= fts_cmd;
        end if;
      when fts_cmd =>
        if cmd_valid = '1' then
          if active_outs /= 0 then --there is output data and          
            ftdi_state <= fts_rd1;
          else
            ftdi_state <= fts_idle;
          end if; 
        end if;
      when fts_rd1 =>
        if rvalid = '1' then         
          ftdi_state <= fts_rd2;
        end if;
      when fts_rd2 =>
        if rvalid = '1' then  
          ftdi_state <= fts_rd3;
        end if;
      when fts_rd3 =>             
        if outs_counter < active_outs-1  then 
          if rvalid = '1' then          
            ftdi_state <= fts_rd1;
          end if;
        else
          if out_fifo_wr_full = 0 then --only if we manage to write, otherwise wait here
            if ft_oe = '1' and ft_rxfe = '0' then -- more to read?
              ftdi_state <= fts_cmd;
            else
              ftdi_state <= fts_idle;
            end if;
          end if;
        end if;
      end case;    
    end if;
  end if; 
end process;

------------------------------------------------------------------------------------------------------------
cmd_valid <= '1' when ftdi_state = fts_cmd and cmd_words(2) = header and ftdi_rd_data = X"00FF" else '0';
------------------------------------------------------------------------------------------------------------
process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then    
    if ftdi_reset = '1' or ftdi_state = fts_idle then 
      cmd_words <= (others => (others => '0'));
    elsif ftdi_state = fts_cmd then
      if rvalid = '1' then  
        if cmd_valid = '0' then
          cmd_words <= cmd_words(1 downto 0) & ftdi_rd_data;
        else        
          cmd_words(2) <= ( X"0000" ); -- invalidate the header but keep the params
        end if;
      end if;
    end if; 
  end if;
end process;
------------------------------------------------------------------------------------------------------------
process (ftdi_clk)
begin
  if rising_edge(ftdi_clk) then    
    if ftdi_reset = '1' then 
      params1  <= (others => '0');
		params2  <= (others => '0');
    elsif cmd_valid = '1' then
      params1 <= cmd_words(1);
		params2 <= cmd_words(0);
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------------------------------------------
with cmd_words(1)(2 downto 0) select active_outs <=
	0  when "000", 
	1  when "001",
	2  when "010",
	4  when "011",
	8  when "100",
	12 when "101",
	16 when "110",
	max_sdo_lines when others; --nr SD Lines = 2x channels

------------------------------------------------------------------------------------------------------------
process (ftdi_clk)
begin
  if rising_edge (ftdi_clk) then
    if ftdi_reset = '1' or cmd_valid = '1' then
      outs_counter <= 0;
    elsif ftdi_state = fts_rd3 and rvalid = '1' and outs_counter < active_outs-1 then 
      outs_counter <= outs_counter+1;
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
process (ftdi_clk)
begin
  if rising_edge (ftdi_clk) then
    if final_rvalid = '0' then 
      if ftdi_state = fts_rd3 and rvalid = '1' then 
        final_rvalid <= '1';
      end if;
    else
      if ftdi_state /= fts_rd3 then
        final_rvalid <= '0';
      end if;
    end if;
  end if;
end process;    
------------------------------------------------------------------------------------------------------------
out_fifo_wr_en <= '1' when 
  ftdi_state = fts_rd3 and
  (rvalid = '1' or final_rvalid = '1' ) and
  outs_counter = active_outs-1 and
  out_fifo_wr_full = 0
  else '0';
------------------------------------------------------------------------------------------------------------
rx_fifos : for i in 0 to max_sdo_lines-1 generate

  process (ftdi_clk)
  begin
    if rising_edge(ftdi_clk) then
      if i = outs_counter and rvalid = '1' then
        case ftdi_state is
        when fts_rd1 => out_fifo_wr_regs(i*2)(15 downto 0) <= ftdi_rd_data;
        when fts_rd2 => out_fifo_wr_regs(i*2)(23 downto 16) <= ftdi_rd_data(7 downto 0);
                        out_fifo_wr_regs((i*2)+1)(7 downto 0) <= ftdi_rd_data(15 downto 8);
        when fts_rd3 => out_fifo_wr_regs((i*2)+1)(23 downto 8) <= ftdi_rd_data;
        when others =>
        end case;
      end if;
    end if;
  end process;
  out_fifo_wr_data(i*2) <= out_fifo_wr_regs(i*2);--right channels
  --left channels need to be muxed from the normal registers to the last read data only for the last channel
  out_fifo_wr_data((i*2)+1) <= out_fifo_wr_regs((i*2)+1) when i /= active_outs-1 else
                               ftdi_rd_data & out_fifo_wr_regs((i*2)+1)(7 downto 0);
  
 end generate;
 
 end behavioral;

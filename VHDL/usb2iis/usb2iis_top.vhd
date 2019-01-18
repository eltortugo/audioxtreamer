----------------------------------------------------------------------------------
-- Company: 
-- Engineer: 
-- 
-- Create Date: 07/12/2018 01:13:41 AM
-- Design Name: 
-- Module Name: FIFO2IIC - Behavioral
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
--use ieee.std_logic_arith.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
library UNISIM;
use UNISIM.VComponents.all;

use work.common_types.all;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
entity CY16_to_IIS is
generic(
sdi_lines   : positive := 16;
sdo_lines   : positive := 16;
bit_depth       : positive := 24
);
  Port (
	 areset : in std_logic;
    --Cypress interface
    cy_clk : in STD_LOGIC; -- 48Mhz
    -- DATA IO
    DIO : inout STD_LOGIC_VECTOR(15 downto 0);
    -- FIFO control
    SLWR, SLRD : out STD_LOGIC;
    SLOE, FIFOADDR0, FIFOADDR1, PKTEND : out STD_LOGIC;
    FLAGA, FLAGB : in STD_LOGIC;
    --LSI ifc
    lsi_clk  : in std_logic;
    lsi_mosi : in std_logic;
    lsi_miso : out std_logic;
    lsi_stop : in std_logic;
    
    
    --yamaha external clock and word clock
    sd_in: in std_logic_vector( sdi_lines-1 downto 0);
    sd_out: out std_logic_vector( sdo_lines-1 downto 0);
    ymh_clk : in std_logic;   
    sd_word_clk : in std_logic;
    
        -- STATUS LEDS
    led: out std_logic_vector (7 downto 0);
	 dbg: out std_logic_vector (7 downto 0)
     
    );
end CY16_to_IIS;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------

architecture Behavioral of CY16_to_IIS is

constant max_nr_outputs : natural := sdo_lines*2;
constant max_nr_inputs : natural := sdi_lines*2;

component ezusb_lsi
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
end component;


signal sd_f256_clk	:std_logic;
signal sd_reset : std_logic; --our reset signal for now.
signal ft_clk	:std_logic;
signal ft_reset : std_logic; --our reset signal for now.

signal rx_data : std_logic_vector(15 downto 0);
signal tx_data : std_logic_vector(15 downto 0);

signal ft_rd, ft_wr, tx_req, tx_grant, ft_oe, ft_nrxf , ft_ntxe : std_logic; 

subtype out_data_array is slv24_array(0 to max_nr_outputs-1);--(bit_depth-1 downto 0);

signal params1 : slv_16;
signal params2 : slv_16;
signal out_fifo_depth : std_logic_vector(6 downto 0);

signal out_fifo_wr_full : std_logic_vector(max_nr_outputs-1 downto 0);
signal out_fifo_wr_en: std_logic;
signal out_fifo_wr_data : out_data_array;

signal sdout_shifter: slv24_array(0 to sdo_lines-1);
signal sdout_load_shifter : std_logic;
signal sdout_is_padding : std_logic;
signal word_clk_reg1: std_logic;
signal word_clk_reg2: std_logic;

type rd_data_count_t is array (natural range <> ) of std_logic_vector(6 downto 0);
signal rd_data_count : rd_data_count_t( 0 to max_nr_outputs-1);

signal out_fifo_rd_data : out_data_array;
signal out_fifo_rd_en : std_logic;
signal out_fifo_rd_empty : std_logic_vector(max_nr_outputs-1 downto 0);

signal sd_counter : std_logic_vector (7 downto 0) := X"00";
signal sd_record_only: std_logic;
signal rec_only_synch: std_logic_vector(1 downto 0);
signal input_shift_enable: std_logic;
signal in_fifo_wr_full : std_logic_vector(max_nr_inputs-1 downto 0);
signal in_fifo_wr_data: slv24_array(0 to max_nr_inputs-1);
signal in_fifo_wr_en : std_logic;

signal in_fifo_rd_empty : std_logic_vector(max_nr_inputs-1 downto 0);
signal in_fifo_rd_data  : slv24_array(0 to max_nr_inputs-1);
signal in_fifo_rd_en : std_logic;
signal in_fifo_empty : std_logic;


signal in_axis_tvalid : std_logic;
signal in_axis_tready : std_logic;
signal in_axis_tdata  : std_logic_vector(15 downto 0);


attribute mark_debug : string;
attribute keep : string;


attribute mark_debug of ft_oe	: signal is "true";
attribute mark_debug of ft_rd	: signal is "true";
attribute mark_debug of ft_wr	: signal is "true";
attribute mark_debug of FLAGA	: signal is "true";
attribute mark_debug of FLAGB	: signal is "true";

------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
begin

ymh_bufg : BUFG port map( I => ymh_clk, O => sd_f256_clk );
cy_bufg :  BUFG port map( I => cy_clk, O => ft_clk );

--parameter OUTEP = 2,            // EP for FPGA -> EZ-USB transfers
--parameter INEP = 6,             // EP for EZ-USB -> FPGA transfers 
--assign FIFOADDR = ( if_out ? (OUTEP/2-1) : (INEP/2-1) ) & 2'b11;
FIFOADDR0 <= '0';
FIFOADDR1 <= ft_oe;

ft_nrxf <= not FLAGA;
ft_ntxe <= not FLAGB;

ezusb_lsi_i : ezusb_lsi
port map
(
  clk => ft_clk,
  reset_in => ft_reset,
  --LSI ifc
  data_clk => lsi_clk,
  miso => lsi_miso,
  mosi => lsi_mosi,
  stop => lsi_stop,
  --registers
  out_data => X"FEEDDEAD",
  out_addr => led
  
  
);

------------------------------------------------------------------------------------------------------------
dio <= tx_data when ft_oe = '0' else (others => 'Z');
rx_data <= dio;
------------------------------------------------------------------------------------------------------------
--bargraph: for i in 1 to led'length-1 generate
--led(i) <= '1' when rd_data_count(0)(6 downto 4) < i else '0';
--end generate;
--led(0) <= out_fifo_rd_empty(0);



------------------------------------------------------------------------------------------------------------
--synchronize the ft_reset to the ft_clk
process (ft_clk, areset )
variable ff: std_logic_vector(2 downto 0);
begin
  if areset = '1' then
      ff := (others => '1');
  elsif rising_edge(ft_clk) then
      ff := ff(ff'length-2 downto 0) & '0'; 
  end if;
  ft_reset <= ff(ff'length-1);
end process;
------------------------------------------------------------------------------------------------------------
--synchronize the sd_reset to the sd_f256_clk
process (sd_f256_clk, areset )
variable ff: std_logic_vector(2 downto 0);
begin
  if areset = '1' then
      ff := (others => '1');      
  elsif rising_edge(sd_f256_clk) then
      ff := ff(ff'length-2 downto 0) & '0'; 
  end if;
  sd_reset <= ff(ff'length-1);
end process;

------------------------------------------------------------------------------------------------------------
bus_arbiter : process(ft_clk)
begin
  if rising_edge(ft_clk) then
    if ft_reset = '1' or ((tx_req = '0' or ft_ntxe = '1') and ft_nrxf = '0' ) then
      ft_oe <= '1'; --always receiving unless
		tx_grant <= '0';
    elsif ft_oe = '1' and tx_req = '1' and (ft_rd = '0' or ft_nrxf = '1')  then
      ft_oe <= '0';
	elsif ft_oe = '0' then
      tx_grant <= '1';    
    end if;
  end if;
end process;

------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------------------------------------------
SLOE <= not ft_oe;
SLRD <= not( ft_rd and ft_oe );
SLWR <= not ft_wr;

dbg(0) <= cy_clk;
dbg(1) <= FLAGA;
dbg(2) <= ft_rd and ft_oe;
dbg(3) <= FLAGB;
dbg(4) <= ft_oe;
dbg(5) <= ft_wr;
dbg(6) <= out_fifo_rd_empty(0);
dbg(7) <= in_fifo_wr_full(0);


------------------------------------------------------------------------------------------------------------
rcvr : entity work.usbf_to_fifo
generic map(
  max_sdo_lines => sdo_lines
)
port map (
  ft_clk  => ft_clk,
  ft_reset  => ft_reset,
  ft_oe   => ft_oe,
  rx_data => rx_data,
  ft_rxfe => ft_nrxf,
  ft_rd  => ft_rd,
  params1  => params1,
  params2  => params2,
  out_fifo_wr_full => out_fifo_wr_full,
  out_fifo_wr_en => out_fifo_wr_en,
  out_fifo_wr_data => out_fifo_wr_data
);
------------------------------------------------------------------------------------------------------------
--using the generator avoids the need to create all the false path contraints for cdc's and async resets

out_fifo_depth <= params1(9 downto 6) &"110"; --default to 64 samples depth

rx_fifos : for i in 0 to max_nr_outputs -1 generate  
  output_fifo_i: entity work.output_fifo
     port map (
      empty  		=> out_fifo_rd_empty(i),
      dout   		=> out_fifo_rd_data(i),
      rd_en			=> out_fifo_rd_en,
      full			=> open,
      din			=> out_fifo_wr_data(i),
      wr_en			=> out_fifo_wr_en,
      rd_data_count	=> rd_data_count(i),
      prog_full	=> out_fifo_wr_full(i),
      prog_full_thresh	=> out_fifo_depth,
		rd_rst	=> sd_reset,
      rd_clk	=> sd_f256_clk,
      wr_rst	=> ft_reset,
      wr_clk	=> ft_clk
    );  
end generate;

------------------------------------------------------------------------------------------------------------
--TBI: add a port to select the padding scheme between left/right/i2s etc

--sdout_is_padding <= '1' when sd_counter(6 downto 2) = 0 or sd_counter(6 downto 2) > bit_depth else '0'; --skip the first bit then msb to lsb and pad the rest
--sdout_is_padding <= '1' when sd_counter(6 downto 5) = 0 else '0'; --pad the first and then msb to lsb (right justified)
sdout_is_padding <= '0'; --left justified
--sdout_is_padding <= not sd_counter(6); --pad the first and then msb to lsb (right justified) just the high 16 bits

----------------------------------------------------------------------------------------------------------
process( sd_f256_clk )
begin    
 if rising_edge(sd_f256_clk) then
	if sd_reset = '1' or  sd_counter /= X"FD" then 
	  out_fifo_rd_en <= '0';
	else
	  out_fifo_rd_en <= '1';
	end if;
 end if;
end process;

----------------------------------------------------------------------------------------------------------

process( sd_f256_clk )
begin    
 if rising_edge(sd_f256_clk) then
	if sd_reset = '1' or  sd_counter(6 downto 0) /= "1111110" then 
	  sdout_load_shifter <= '0';
	else
	  sdout_load_shifter <= '1';
	end if;
 end if;
end process;
  

----------------------------------------------------------------------------------------------------------
output_shifter : for i in 0 to sdo_lines-1 generate
-- load a 24 shift register and clock out
  sd_out(i) <= '0' when sdout_is_padding = '1' else sdout_shifter(i)(23);
  process( sd_f256_clk )
  begin    
    if rising_edge(sd_f256_clk) then
      if sd_reset = '1' then 
        sdout_shifter(i) <= (others =>'0');
      elsif sdout_load_shifter = '1' then
        if sd_counter(7) = '0' then --we are loading the shifter one clock before the start of L , when LR is 1
          if  out_fifo_rd_empty(i*2) = '0' then
            sdout_shifter(i) <= out_fifo_rd_data(i*2);
          else
            sdout_shifter(i) <= (others => '0');
          end if;
        elsif out_fifo_rd_empty((i*2)+1) = '0' then
          sdout_shifter(i) <= out_fifo_rd_data((i*2)+1);
        end if;
      elsif sdout_is_padding = '0' and sd_counter(1 downto 0) = "11" then
        sdout_shifter(i) <= sdout_shifter(i)(bit_depth -2 downto 0) & '0'; 
      end if;
    end if;
  end process;
end generate;
------------------------------------------------------------------------------------------------------------
 -- n256 counter triggered by falling edge of the input word clock.
fs256_counter : process ( sd_f256_clk )

begin
  if rising_edge (sd_f256_clk) then

    word_clk_reg1 <= sd_word_clk; -- sync LR
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
--Inputs to fifo
--synchronize the configuration registers to the sd_f256_clk domain
process (sd_f256_clk, params1, sd_reset, rec_only_synch )
variable outs_active: std_logic := '0';
begin
  if params1(2 downto 0) = 0 then
    outs_active := '0';
  else
    outs_active := '1';
  end if;
  
  if sd_reset = '1' then
      rec_only_synch <= (others => '0');      
  elsif rising_edge(sd_f256_clk) then
      rec_only_synch <= rec_only_synch(rec_only_synch'length-2 downto 0) & outs_active; 
  end if;
  sd_record_only <= rec_only_synch(rec_only_synch'length-1);
end process;



-- msb left(justified)
input_shift_enable <= '1' when sd_counter(6 downto 2) < bit_depth and sd_counter(1 downto 0) = "01" else '0';
in_fifo_wr_en <= '1' when sd_counter = X"FE" and ( sd_record_only = '1' or out_fifo_rd_empty = 0) else '0';

input_shifter : for i in 0 to sdi_lines-1 generate
  process(sd_f256_clk)
  begin
    if rising_edge(sd_f256_clk) then
      if sd_reset = '0' and input_shift_enable = '1' then
        if sd_counter(7) = '1' then
          in_fifo_wr_data(i*2) <= in_fifo_wr_data(i*2)(bit_depth-2 downto 0) & sd_in(i);
        elsif (max_nr_inputs mod 2) = 0 then
          in_fifo_wr_data(i*2+1) <= in_fifo_wr_data(i*2+1)(bit_depth-2 downto 0) & sd_in(i);
        end if;
      end if;
    end if;
  end process;
end generate;
------------------------------------------------------------------------------------------------------------

tx_fifos : for i in 0 to max_nr_inputs-1 generate
  
    input_fifo_i : entity work.input_fifo
     port map (
      empty 	=> in_fifo_rd_empty(i),
      dout 		=> in_fifo_rd_data(i),
      rd_en 	=> in_fifo_rd_en,
      full  	=> in_fifo_wr_full(i),
      din		=> in_fifo_wr_data(i),
      wr_en 	=> in_fifo_wr_en,
		rd_rst	=> ft_reset,
      rd_clk	=> ft_clk,
      wr_rst	=> sd_reset,
      wr_clk	=> sd_f256_clk
    );
end generate;
------------------------------------------------------------------------------------------------------------

in_fifo_empty <= '0' when in_fifo_rd_empty = 0 else '1';

xmtr: entity work.fifo_to_m_axis
generic map (
  max_sdi_lines => sdi_lines
) port map (
  --FTDI interface
  clk  => ft_clk,
  reset=> ft_reset,

  -- status report
  status_1 => '0' & rd_data_count(0),
  status_2 => X"29",
  params1  => params1,
  params2  => params2,

  in_fifo_empty => in_fifo_empty,
  in_fifo_rd    => in_fifo_rd_en,
  in_fifo_data  => in_fifo_rd_data,

  m_axis_tvalid => in_axis_tvalid,
  --m_axis_tlast
  m_axis_tready => in_axis_tready,
  m_axis_tdata  => in_axis_tdata
);

inst_s_axis_to_w_fifo: entity work.s_axis_to_w_fifo
  generic map (
    DATA_WIDTH => 16
  ) port map (
    clk  => ft_clk,
    reset=> ft_reset,

    tx_grant   => tx_grant,
    tx_req  => tx_req,
    tx_data => tx_data,
    tx_full => ft_ntxe,
    tx_wr  => ft_wr,
    pktend => pktend,

    s_axis_tvalid => in_axis_tvalid,
    --s_axis_tlast
    s_axis_tready => in_axis_tready,
    s_axis_tdata  => in_axis_tdata
);

end Behavioral;



























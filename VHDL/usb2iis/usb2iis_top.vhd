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
use ieee.std_logic_arith.all;

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
sdo_lines   : positive := 16
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
    ain: in std_logic_vector( 11 downto 0);
    aout: out std_logic_vector( 11 downto 0);

    txb_oe: out std_logic;

    spdif_out1 : out std_logic;
    ymh_clk : in std_logic;
    ymh_word_clk : in std_logic;

    led: out std_logic_vector (7 downto 0)

    );
end CY16_to_IIS;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------

architecture Behavioral of CY16_to_IIS is

constant max_nr_outputs : natural := sdo_lines*2;
constant max_nr_inputs : natural := sdi_lines*2;

function to_std_logic_vector( str : string )
  return std_logic_vector
  is
  variable vec : std_logic_vector( str'length * 8 - 1 downto 0) ;
begin
  for i in 1 to str'high loop
    vec(i * 8 - 1 downto (i - 1) * 8) := std_logic_vector( to_unsigned( character'pos(str(i)) , 8 ) ) ;
  end loop ;
  return vec ;
end function ;

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

signal f256_clk	:std_logic;
signal word_clk	:std_logic;

signal sd_reset : std_logic;
signal usb_clk	:std_logic;
signal usb_reset : std_logic;
signal io_reset : std_logic;
signal clkgen_locked : std_logic;

signal rx_data : std_logic_vector(15 downto 0);
signal tx_data : std_logic_vector(15 downto 0);

signal ft_rd, ft_wr, tx_req, tx_grant, ft_oe, ft_nrxf , ft_ntxe : std_logic; 

subtype out_data_array is slv24_array(0 to max_nr_outputs-1);
signal out_fifo_init: std_logic;
signal out_fifo_full : std_logic_vector(max_nr_outputs-1 downto 0);
signal out_prog_full : std_logic_vector(max_nr_outputs-1 downto 0);
signal out_fifo_wr_en: std_logic;
signal out_fifo_din : out_data_array;

signal rcvr_wr: std_logic;
signal rcvr_data : out_data_array;
signal rcvr_fifo_full : std_logic;
signal rcvr_cmd_valid : std_logic;

signal unlock : std_logic;
signal is_streaming : std_logic;

signal sd_in : std_logic_vector(sdi_lines-1 downto 0);
signal sd_out : std_logic_vector(sdo_lines-1 downto 0);

type rd_data_count_t is array (natural range <> ) of std_logic_vector(7 downto 0);
signal rd_data_count : rd_data_count_t( 0 to max_nr_outputs-1);

signal out_fifo_rd_data : out_data_array;
signal out_fifo_rd_en : std_logic;
signal out_fifo_rd : std_logic;
signal out_fifo_empty : std_logic_vector(max_nr_outputs-1 downto 0);
signal out_empty  : std_logic;


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

signal lsi_rd_data : slv_32;
signal lsi_rd_addr : slv_8;

signal lsi_wr_data : slv_32;
signal lsi_wr_addr : slv_8;
signal lsi_wr   : std_logic;

signal reg_sr_count : slv_32;
signal reg_debug : slv_32;
signal reg_ch_params : slv_32;



constant cookie_str :string := "TRTG";
constant cookie : slv_32 := to_std_logic_vector(cookie_str);

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
txb_oe <= '1';
sd_in <= "0000" & ain;
aout <= sd_out(11 downto 0);

ymh_bufg : BUFG port map( I => ymh_clk, O => f256_clk );
cy_bufg :  BUFG port map( I => cy_clk, O => usb_clk );

------------------------------------------------------------------------------------------------------------

--clock_gen : entity work.clockgen
--port map
--(
--  clk_48m => usb_clk,
--  clk_12m288 => f256_clk,
--  reset => usb_reset,
--  locked => clkgen_locked,
--  word_clk => word_clk
--);
word_clk <= ymh_word_clk;
clkgen_locked <= areset;
------------------------------------------------------------------------------------------------------------

ezusb_lsi_i : ezusb_lsi
port map
(
  clk => usb_clk,
  reset_in => usb_reset,
  --LSI ifc
  data_clk => lsi_clk,
  miso => lsi_miso,
  mosi => lsi_mosi,
  stop => lsi_stop,
  --registers
  out_data => lsi_rd_data,
  out_addr => lsi_rd_addr,

  in_strobe => lsi_wr,
  in_data => lsi_wr_data,
  in_addr => lsi_wr_addr
);

------------------------------------------------------------------------------------------------------------

with lsi_rd_addr select lsi_rd_data <=  
  cookie when X"00", -- the cookie
  X"00000001" when X"01", -- the current version of the fpga
  reg_sr_count when X"02", -- sampling rate counter to detect the word clock
  reg_debug    when X"03",
  reg_ch_params when X"04",

  X"CACABACA" when others;

------------------------------------------------------------------------------------------------------------

proc_reg_debug : process(usb_clk)
begin
  if rising_edge(usb_clk) then
    reg_debug <= ext( FLAGB  &FLAGA & sd_reset & io_reset & clkgen_locked, reg_debug'length );
  end if;
end process;

------------------------------------------------------------------------------------------------------------
sr_detect : entity work.SamplingRateDetect
  port map
(
  clk => usb_clk,
  rst => usb_reset,
  word_clk => word_clk,
  sr_count => reg_sr_count
);

------------------------------------------------------------------------------------------------------------
iobufs : for i in 0 to 15 generate
begin
  iobuf_n : iobuf 
  port map(
    O => rx_data(i),
    IO => dio(i),
    I => tx_data(i),
    T => ft_oe
  );
end generate;

--dio <= tx_data when ft_oe = '0' else (others => 'Z');
--rx_data <= dio;
------------------------------------------------------------------------------------------------------------
bargraph: for i in 1 to led'length-1 generate
led(i) <= '1' when rd_data_count(0)(6 downto 4) < i else '0';
end generate;
led(0) <= out_empty;

------------------------------------------------------------------------------------------------------------
--synchronize the ft_reset to the ft_clk
process (usb_clk, areset )
variable ff: std_logic_vector(2 downto 0);
begin
  if areset = '1' then
      ff := (others => '1');
  elsif rising_edge(usb_clk) then
      ff := ff(ff'length-2 downto 0) & '0'; 
  end if;
  usb_reset <= ff(ff'length-1);
end process;
------------------------------------------------------------------------------------------------------------
--synchronize the sd_reset to the sd_f256_clk
process (f256_clk, areset )
variable ff: std_logic_vector(2 downto 0);
begin
  if areset = '1' then
      ff := (others => '1');
  elsif rising_edge(f256_clk) then
      ff := ff(ff'length-2 downto 0) & '0'; 
  end if;
  sd_reset <= ff(ff'length-1);
end process;

------------------------------------------------------------------------------------------------------------
bus_arbiter : process(usb_clk)
begin
  if rising_edge(usb_clk) then
    if usb_reset = '1' or ((tx_req = '0' or ft_ntxe = '1') and ft_nrxf = '0' ) then
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
SLOE <= not ft_oe;
SLRD <= not( ft_rd and ft_oe );
SLWR <= not ft_wr;

--parameter OUTEP = 2,            // EP for FPGA -> EZ-USB transfers
--parameter INEP = 6,             // EP for EZ-USB -> FPGA transfers 
--assign FIFOADDR = ( if_out ? (OUTEP/2-1) : (INEP/2-1) ) & 2'b11;
FIFOADDR0 <= '0';
FIFOADDR1 <= ft_oe;

ft_nrxf <= not FLAGA;
ft_ntxe <= not FLAGB;
------------------------------------------------------------------------------------------------------------
proc_settings : process(usb_clk)
  variable resetting : std_logic := '0';
begin
  if rising_edge(usb_clk) then
    if usb_reset = '1' then
      out_fifo_init <= '0';
      reg_ch_params <= (others => '0');
      is_streaming <= '0';
      unlock <= '0';
    elsif lsi_wr = '1' and lsi_wr_addr = X"04" then
      reg_ch_params <= lsi_wr_data;
      out_fifo_init <= '1';
      resetting := '1';
      is_streaming <= '0';
    elsif resetting = '1' and (out_fifo_full or out_prog_full) = 0 then --fifo deasserted the prog_full flag after reset so we can check for assertion
      resetting := '0';
    elsif resetting = '0' and out_fifo_init = '1' and (out_fifo_full or out_prog_full) /= 0 then
      out_fifo_init <= '0';
      unlock <= '1';
    elsif resetting = '0' and out_fifo_init = '0' then
      if rcvr_cmd_valid = '1' then
        is_streaming <= '1';
      end if;
      unlock <= '0';
    end if;
  end if;
end process;

rcvr_fifo_full <= '0' when unlock = '1' else '1' when out_fifo_init = '1' or (out_fifo_full or out_prog_full) /= 0 else '0';

------------------------------------------------------------------------------------------------------------
rcvr : entity work.usbf_to_fifo
generic map(
  max_sdo_lines => sdo_lines
)
port map (
  usb_clk  => usb_clk,
  reset  => io_reset,
  usb_oe   => ft_oe,
  usb_data => rx_data,
  usb_empty => ft_nrxf,
  usb_rd_req  => ft_rd,
  nr_outputs  => reg_ch_params(3 downto 0),
  out_fifo_full => rcvr_fifo_full,
  out_fifo_wr   => rcvr_wr,
  out_fifo_data => rcvr_data,
  valid_packet => rcvr_cmd_valid
);
------------------------------------------------------------------------------------------------------------
io_reset <= '1' when  usb_reset = '1' or (lsi_wr = '1' and lsi_wr_addr = X"04") else '0';
------------------------------------------------------------------------------------------------------------
out_fifo_wr_en <= out_fifo_init or rcvr_wr;
out_empty <= out_fifo_empty(0);
out_fifo_din <= rcvr_data when out_fifo_init = '0' else (others => (others => '0'));
out_fifo_rd <= out_fifo_rd_en and is_streaming;
------------------------------------------------------------------------------------------------------------
out_fifos : for i in 0 to max_nr_outputs -1 generate
  output_fifo_i: entity work.output_fifo
     port map (
      empty  		=> out_fifo_empty(i),
      dout   		=> out_fifo_rd_data(i),
      rd_en			=> out_fifo_rd,
      full			=> out_fifo_full(i),
      din			=> out_fifo_din(i),
      wr_en			=> out_fifo_wr_en,
      rd_data_count	=> rd_data_count(i),
      prog_full	=> out_prog_full(i),
      prog_full_thresh(7 downto 4)  => reg_ch_params(23 downto 20),
      prog_full_thresh(3 downto 0)  => "1110",
      rd_rst	=> sd_reset,
      rd_clk	=> f256_clk,
      wr_rst	=> io_reset,
      wr_clk	=> usb_clk
    );  
end generate;
------------------------------------------------------------------------------------------------------------
pcmio_inst: entity work.pcmio
  generic map (
    sdo_lines        => sdo_lines,
    sdi_lines        => sdi_lines
  )
  port map (
    f256_clk         => f256_clk,
    sd_reset         => sd_reset,
    word_clk         => word_clk,
    spdif_out1       => spdif_out1,
    out_fifo_rd_data => out_fifo_rd_data,
    out_empty        => out_empty,
    out_fifo_rd_en   => out_fifo_rd_en,
    in_fifo_wr_data  => in_fifo_wr_data,
    in_fifo_wr_en    => in_fifo_wr_en,
    sd_in            => sd_in,
    sd_out           => sd_out
  );

------------------------------------------------------------------------------------------------------------
in_fifos : for i in 0 to max_nr_inputs-1 generate
  
    input_fifo_i : entity work.input_fifo
     port map (
      empty 	=> in_fifo_rd_empty(i),
      dout 		=> in_fifo_rd_data(i),
      rd_en 	=> in_fifo_rd_en,
      full  	=> in_fifo_wr_full(i),
      din		=> in_fifo_wr_data(i),
      wr_en 	=> in_fifo_wr_en,
      rd_rst	=> io_reset,
      rd_clk	=> usb_clk,
      wr_rst	=> sd_reset,
      wr_clk	=> f256_clk
    );
end generate;
------------------------------------------------------------------------------------------------------------

in_fifo_empty <= '0' when in_fifo_rd_empty = 0 else '1';

xmtr: entity work.fifo_to_m_axis
generic map (
  max_sdi_lines => sdi_lines
) port map (
  --cypress interface
  clk  => usb_clk,
  reset=> io_reset,

  -- status report
  status_1 => rd_data_count(0),
  status_2 => X"29",
  nr_inputs => reg_ch_params(7 downto 4),
  nr_padding => reg_ch_params(31 downto 24),
  nr_samples => reg_ch_params(15 downto 8),
  tx_enable  => is_streaming,

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
    clk  => usb_clk,
    reset=> io_reset,

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

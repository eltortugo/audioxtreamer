------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

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
entity top_audioxtreamer is
generic(
sdi_lines   : positive := 12;
sdo_lines   : positive := 12
);
  Port (
    areset      : in std_logic;
    --Cypress interface
    cy_clk      : in STD_LOGIC; -- 48Mhz
    DIO         : inout STD_LOGIC_VECTOR(15 downto 0);
    -- FIFO control
    SLWRn, SLRDn, SLOEn : out STD_LOGIC;
    FIFOADDR0, FIFOADDR1, PKTEND : out STD_LOGIC;
    FLAGA, FLAGB: in STD_LOGIC;
    --lsi8
    lsi8_rdnck  : in std_logic;
    lsi8_en     : in std_logic;
    lsi8_dio    : inout std_logic_vector(7 downto 0);

    ez_int      : out std_logic;
    ez_bsy      : out std_logic;
    ez_sof      : in  std_logic;
    --yamaha external clock and word clock
    ain         : in std_logic_vector( 11 downto 0);
    aout        : out std_logic_vector( 11 downto 0);
    txb_oe      : out std_logic;
    spdif_out1  : out std_logic;
    ymh_clk     : in std_logic;
    ymh_word_clk: in std_logic;

    midi_rx     : in std_logic_vector( 5 downto 1 );
    midi_tx     : out std_logic_vector( 5 downto 1 )
    );
end top_audioxtreamer;
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------

architecture rtl of top_audioxtreamer is

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

signal f256_clk	:std_logic;
signal word_clk	:std_logic;

signal sd_reset : std_logic;
signal usb_clk	:std_logic;
signal usb_reset : std_logic;
signal io_reset : std_logic;
--signal clkgen_locked : std_logic;

subtype out_data_array is slv24_array(0 to max_nr_outputs-1);

signal out_fifo_full : std_logic_vector((max_nr_outputs/3)-1 downto 0);
signal out_prog_full : std_logic_vector((max_nr_outputs/3)-1 downto 0);

signal rcvr_wr: std_logic;
signal rcvr_data : out_data_array;
signal rcvr_fifo_full : std_logic;
signal rcvr_fifo_full_sync :std_logic;

signal tx_reset : std_logic;

signal sd_in : std_logic_vector(sdi_lines-1 downto 0);
signal sd_out : std_logic_vector(sdo_lines-1 downto 0);

type rd_data_count_t is array (natural range <> ) of std_logic_vector(7 downto 0);
signal rd_data_count : rd_data_count_t( 0 to max_nr_outputs-1);

signal out_axis_tvalid : std_logic;
signal out_axis_tready : std_logic;
signal out_axis_tdata  : std_logic_vector(15 downto 0);


signal out_fifo_rd_data : out_data_array;
signal out_fifo_rd_en : std_logic;
signal out_fifo_rd : std_logic;
signal out_fifo_empty : std_logic_vector((max_nr_outputs/3)-1 downto 0);
signal out_empty  : std_logic;
--signal out_fifo_min : std_logic_vector(7 downto 0);
signal out_fifo_skip : std_logic;
signal out_fifo_skip_count : slv_16;
--signal out_fifo_stats_reset : std_logic;

signal in_fifo_full_count : slv_16;
signal in_fifo_wr_full : std_logic_vector((max_nr_inputs/3)-1 downto 0);
signal in_fifo_wr_data: slv24_array(0 to max_nr_inputs-1);
signal in_fifo_wr_en : std_logic;

signal in_fifo_rd_empty : std_logic_vector((max_nr_inputs/3)-1 downto 0);
signal in_fifo_rd_data: slv24_array(0 to max_nr_inputs-1);
signal in_fifo_rd_en  : std_logic;
signal in_fifo_empty  : std_logic;

signal in_axis_tvalid : std_logic;
signal in_axis_tready : std_logic;
signal in_axis_tlast  : std_logic;
signal in_axis_tdata  : std_logic_vector(15 downto 0);


signal lsi_rd_data    : slv_32;
signal lsi_rd_addr    : slv_8;
signal lsi_rd         : std_logic;
signal lsi_rd_done    : std_logic;

signal lsi_wr_data    : slv_32;
signal lsi_wr_addr    : slv_8;
signal lsi_wr         : std_logic;


signal reg_sr_count   : slv_16;
signal reg_ch_params  : slv_32;

signal midi_in_data   : slv32_array(1 to 7) ;
signal midi_in_valid  : std_logic_vector(7 downto 1);
signal midi_in_ready  : std_logic_vector(7 downto 1);
signal midi_in_valid_r: std_logic_vector(7 downto 1);

signal midi_out_valid : std_logic;
signal midi_out_ready : std_logic;
signal midi_out_busy  : std_logic;
signal midi_out_sink  : std_logic_vector(1 downto 0);

signal spmf   : slv_32;
signal spmf_t : std_logic;
signal sof_r  : std_logic_vector(2 downto 0);


constant cookie_str :string := "TRTG";
constant cookie : slv_32 := to_std_logic_vector(cookie_str);


------------------------------------------------------------------------------------------------------------
begin
txb_oe <= '1';
sd_in <= ain;
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
--clkgen_locked <= areset;
------------------------------------------------------------------------------------------------------------


ez_int <= '0' when midi_in_valid = 0 else '1';

------------------------------------------------------------------------------------------------------------

with lsi_rd_addr select lsi_rd_data <=
  cookie when X"00", -- the cookie
  X"00000001" when X"01", -- the current version of the fpga
  x"00" & rd_data_count(0) & reg_sr_count when X"02",           -- sampling rate counter to detect the word clock and the out fifo level
  in_fifo_full_count & out_fifo_skip_count when X"03",  -- fifo empty /full counters

  spmf when X"08",

  X"000000"& '0' & midi_in_valid_r when X"10" | X"20",
  midi_in_data(1) when X"11" | X"21",
  midi_in_data(2) when X"12" | X"22",
  midi_in_data(3) when X"13" | X"23",
  midi_in_data(4) when X"14" | X"24",
  midi_in_data(5) when X"15" | X"25",
  midi_in_data(6) when X"16" | X"26",
  midi_in_data(7) when X"17" | X"27",

  X"CACABACA" when others;

------------------------------------------------------------------------------------------------------------

sr_detect : entity work.sampling_rate_detect
  port map (
    clk => usb_clk,
    rst => usb_reset,
    pcm_clk => f256_clk,
    sr_count => reg_sr_count
  );

-----------------------------------------------------------------------------------------------------------

p_sof: process(usb_clk)
begin
  if rising_edge(usb_clk) then
    sof_r <= sof_r(1 downto 0) & ez_sof;
    if usb_reset = '1' then
      sof_r <= (others => '0');
    else
      spmf_t <= '0';
      if sof_r (2) = '0' and sof_r (1) = '1' then
        spmf_t <= '1';
      end if;
    end if;
  end if;
end process;

--spmf_t <= '1' when lsi_rd_addr = X"08" and lsi_rd = '1' else '0';

sample_counter_inst: entity work.sample_counter
  generic map ( trig_freq => 32)
  port map (
    clk   => usb_clk,
    rst   => usb_reset,
    f256  => f256_clk,
    trig  => spmf_t,
    spmf  => spmf
  );

-----------------------------------------------------------------------------------------------------------
ez_lsi8_inst: entity work.ez_lsi8
  port map (
    clk         => usb_clk,
    rst         => usb_reset,
    lsi8_rdnck  => lsi8_rdnck,
    lsi8_en     => lsi8_en,
    lsi8_dio    => lsi8_dio,
    lsi_rd_data => lsi_rd_data,
    lsi_rd_addr => lsi_rd_addr,
    lsi_rd      => lsi_rd,
    lsi_rd_done => lsi_rd_done,
    lsi_wr_data => lsi_wr_data,
    lsi_wr_addr => lsi_wr_addr,
    lsi_wr      => lsi_wr
  );

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
proc_settings : process(usb_clk)
begin
  if rising_edge(usb_clk) then
    if usb_reset = '1' then
      reg_ch_params <= (others => '0');
    elsif lsi_wr = '1' then
      if lsi_wr_addr = X"04" then
        reg_ch_params <= lsi_wr_data;
      end if;
    end if;
  end if;
end process;

------------------------------------------------------------------------------------------------------------

rcvr_fifo_full <= '1' when (out_fifo_full or out_prog_full) /= 0 else '0';

------------------------------------------------------------------------------------------------------------

--proc_out_fifo_min : process (usb_clk)
--begin
--  if rising_edge(usb_clk) then
--    if usb_reset = '1' or out_fifo_stats_reset = '1' then
--      out_fifo_min <= (others => '1');
--	 elsif out_empty = '1' then
--	   out_fifo_min <= (others => '0');
--   elsif rd_data_count(0) < out_fifo_min then
--	   out_fifo_min <= rd_data_count(0);
--	 end if;
--  end if;
--end process;


proc_out_fifo_skip : process (f256_clk)
begin
  if rising_edge(f256_clk) then
    if sd_reset = '1' or tx_reset = '1' then
      out_fifo_skip_count <= (others => '0');
      out_fifo_skip <= '0';
    elsif rd_data_count(0) = 0 and out_fifo_rd = '1' and out_fifo_skip_count < X"FFFF" then
      out_fifo_skip_count <= out_fifo_skip_count +1;
      out_fifo_skip <= '1';
   else
      out_fifo_skip <= '0';
    end if;
  end if;
end process;
------------------------------------------------------------------------------------------------------------
proc_in_fifo_full : process (f256_clk)
begin
  if rising_edge(f256_clk) then
    if sd_reset = '1' or tx_reset = '1' then
      in_fifo_full_count <= (others => '0');
    elsif in_fifo_wr_full /= 0 and in_fifo_wr_en = '1' and in_fifo_full_count < X"FFFF" then
	   in_fifo_full_count <= in_fifo_full_count +1;
	 end if;
  end if;
end process;

------------------------------------------------------------------------------------------------------------
rcvr : entity work.isoch_audio_out
generic map(
  max_sdo_lines => sdo_lines
)
port map (
  usb_clk  => usb_clk,
  reset  => io_reset,

  s_axis_tdata  => out_axis_tdata,
  s_axis_tvalid => out_axis_tvalid,
  s_axis_tready => out_axis_tready,

  nr_outputs    => reg_ch_params(7 downto 0),
  out_fifo_full => rcvr_fifo_full,
  out_fifo_wr   => rcvr_wr,
  out_fifo_data => rcvr_data

  --,uf_pulse      => spmf_t
);
------------------------------------------------------------------------------------------------------------
io_reset <= '1' when  usb_reset = '1' or reg_ch_params = 0 or (lsi_wr = '1' and lsi_wr_addr = X"04") else '0';
------------------------------------------------------------------------------------------------------------



------------------------------------------------------------------------------------------------------------
-- fill buffer: when the fifo is empty, wait until it is fully filled before starting to read
-- reducing the depth will increase drops by a bursty data flow so if the sender can keep the variation
-- the data flow smooth (5 & 6 samples per microframe "spp" @44k1) the out fifo can go as low as 8 samples

p_fill_out : process (f256_clk,out_fifo_rd_en)
  variable fill : std_logic := '1';
  variable full_sync : std_logic_vector(2 downto 0) := "000";
begin

  if rising_edge(f256_clk) then
    if sd_reset = '1' then
      fill := '1';
      rcvr_fifo_full_sync <= '0';
    else
      full_sync := full_sync(1 downto 0) & rcvr_fifo_full;
      rcvr_fifo_full_sync <= full_sync(2);
      if fill = '1' and  rcvr_fifo_full_sync = '0' and full_sync(2) = '1' then --edge
        fill := '0';
      elsif fill = '0' and out_fifo_empty(0) = '1' then
        fill := '1';
      end if;
    end if;
  end if;

  out_empty <= fill;

  if fill = '0' then
    out_fifo_rd <= out_fifo_rd_en;
  else
    out_fifo_rd <= '0';
  end if;
end process;
------------------------------------------------------------------------------------------------------------

g_out_fifos : for i in 0 to (max_nr_outputs/3) -1 generate

  output_fifo_i: entity work.ASYNCH_FIFO_72x256
    port map (
      empty      => out_fifo_empty(i),

      dout(23 downto  0)  => out_fifo_rd_data(i*3),
      dout(47 downto 24)  => out_fifo_rd_data(i*3+1),
      dout(71 downto 48)  => out_fifo_rd_data(i*3+2),

      rd_en      => out_fifo_rd,

      full      => out_fifo_full(i),

      din(23 downto  0)  => rcvr_data(i*3),
      din(47 downto 24)  => rcvr_data(i*3+1),
      din(71 downto 48)  => rcvr_data(i*3+2),

      wr_en      => rcvr_wr,
      rd_data_count  => rd_data_count(i),

      prog_full  => out_prog_full(i),
      prog_full_thresh => reg_ch_params(23 downto 16),
      --prog_full_thresh(8 downto 1) => reg_ch_params(23 downto 16),
      --prog_full_thresh(0) => '0',
      rd_rst  => sd_reset,
      rd_clk  => f256_clk,
      wr_rst  => io_reset,
      wr_clk  => usb_clk
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
in_fifos : for i in 0 to (max_nr_inputs/3)-1 generate
  
    input_fifo_i : entity work.ASYNCH_FIFO_72x256
     port map (
      empty 	=> in_fifo_rd_empty(i),

      dout(23 downto  0)  => in_fifo_rd_data(i*3),
      dout(47 downto 24)  => in_fifo_rd_data(i*3+1),
      dout(71 downto 48)  => in_fifo_rd_data(i*3+2),

      rd_en   => in_fifo_rd_en,
      full    => in_fifo_wr_full(i),
      prog_full_thresh => (others => '1'),
      din(23 downto  0)  => in_fifo_wr_data(i*3),
      din(47 downto 24)  => in_fifo_wr_data(i*3+1),
      din(71 downto 48)  => in_fifo_wr_data(i*3+2),

      wr_en 	=> in_fifo_wr_en,
      rd_rst	=> io_reset,
      rd_clk	=> usb_clk,
      wr_rst	=> sd_reset,
      wr_clk	=> f256_clk
    );
end generate;
------------------------------------------------------------------------------------------------------------

in_fifo_empty <= '0' when in_fifo_rd_empty = 0 else '1';

------------------------------------------------------------------------------------------------------------

--latch the channels when reading @ 0005 to avoid sending data arrived after that
p_midi_in_valid : process(usb_clk)
begin
  if rising_edge(usb_clk) then
    if lsi_rd_addr = X"10" and lsi_rd = '1' then
      midi_in_valid_r <= midi_in_valid;
    end if;
  end if;
end process;


p_midi_in_ready : process(lsi_rd_addr,lsi_rd_done)

begin
  midi_in_ready <= (others=>'0');
  case lsi_rd_addr is
    when X"11"  => midi_in_ready(1) <= lsi_rd_done;
    when X"12"  => midi_in_ready(2) <= lsi_rd_done;
    when X"13"  => midi_in_ready(3) <= lsi_rd_done;
    when X"14"  => midi_in_ready(4) <= lsi_rd_done;
    when X"15"  => midi_in_ready(5) <= lsi_rd_done;
    when X"16"  => midi_in_ready(6) <= lsi_rd_done;
    when X"17"  => midi_in_ready(7) <= lsi_rd_done;
    when others => null;
  end case;
end process;

------------------------------------------------------------------------------------------------------------
tx_reset <= '1' when reg_ch_params = 0 else '0';
------------------------------------------------------------------------------------------------------------
xmtr: entity work.isoch_audio_in
generic map (
  max_sdi_lines => sdi_lines
) port map (
  --cypress interface
  clk  => usb_clk,
  reset=> tx_reset,

  nr_inputs       => reg_ch_params(15 downto 8),
  sof_int         => '0',

  in_fifo_empty   => in_fifo_empty,
  in_fifo_rd      => in_fifo_rd_en,
  in_fifo_data    => in_fifo_rd_data,

  m_axis_tvalid   => in_axis_tvalid,
  m_axis_tlast    => in_axis_tlast,
  m_axis_tready   => in_axis_tready,
  m_axis_tdata    => in_axis_tdata
);
------------------------------------------------------------------------------------------------------------
i_s_axis_to_w_fifo: entity work.s_axis_to_w_fifo
  generic map (
    DATA_WIDTH => 16
  ) port map (
    clk  => usb_clk,
    reset=> usb_reset,

-- FX2LP ifc
    DIO => DIO,

    FIFOADDR0 => FIFOADDR0,
    FIFOADDR1 => FIFOADDR1,
    FLAGA => FLAGA,
    FLAGB => FLAGB,

    SLRDn => SLRDn,
    SLWRn => SLWRn,
    PKTEND => PKTEND,
    SLOEn => SLOEn,

--s_axis m_axis

    m_axis_tdata  => out_axis_tdata,
    m_axis_tvalid => out_axis_tvalid,
    m_axis_tready => out_axis_tready,

    s_axis_tvalid => in_axis_tvalid,
    s_axis_tlast  => in_axis_tlast,
    s_axis_tready => in_axis_tready,
    s_axis_tdata  => in_axis_tdata
  );
------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------
--lsi_8 to axis adapter, is just a latch/busy flag to inform the ezusb when the command has not been dispatched

my_process_ssp: process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if usb_reset = '1' or midi_out_ready = '1' then
      midi_out_busy <= '0';
    elsif lsi_wr ='1' and lsi_wr_addr = x"10" and midi_out_ready = '0' then
      midi_out_busy <= '1';
    end if;
  end if;
end process;

ez_bsy <= midi_out_busy;

midi_out_valid <= '1' when (lsi_wr ='1' and lsi_wr_addr = x"10") or midi_out_busy = '1' else '0';
------------------------------------------------------------------------------------------------------------
i_midi_io : entity work.midi_io
generic map ( NR_CHANNELS => 7 )
port map
(
  clk             => usb_clk,
  reset           => usb_reset,

  rx(5 downto 1)  => midi_rx,
  rx(7 downto 6)  => "00",
  rx_data         => midi_in_data,
  rx_valid        => midi_in_valid,
  rx_ready        => midi_in_ready,
  rx_fifo_reset   => usb_reset,

  tx(5 downto 1)  => midi_tx,
  tx(7 downto 6)  => midi_out_sink,
  tx_data         => lsi_wr_data,
  tx_valid        => midi_out_valid,
  tx_ready        => midi_out_ready
);

------------------------------------------------------------------------------------------------------------
end rtl;

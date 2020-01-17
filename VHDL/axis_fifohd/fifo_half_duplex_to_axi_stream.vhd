------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library IEEE;
  use IEEE.STD_LOGIC_1164.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
  use IEEE.NUMERIC_STD.all;
  use ieee.std_logic_unsigned.all;

library UNISIM;
use UNISIM.VComponents.all;


------------------------------------------------------------------------------------------------------------
entity s_axis_to_w_fifo is
  generic(
    DATA_WIDTH  : positive := 16
  );
  port (
    --USB interface
    clk : in STD_LOGIC;
    reset : in STD_LOGIC;
    -- DATA IO

     DIO: inout STD_LOGIC_VECTOR(DATA_WIDTH-1 downto 0);
    -- FIFO control

    SLRDn     : out std_logic;
    SLWRn     : out std_logic;
    SLOEn     : out std_logic;
    FIFOADDR0 : out std_logic;
    FIFOADDR1 : out std_logic;
    FLAGA     : in std_logic;
    FLAGB     : in std_logic;
    PKTEND    : out std_logic;

--AXIS Master out endpoint
    m_axis_tdata : out STD_LOGIC_VECTOR(DATA_WIDTH-1 downto 0);
    m_axis_tready: in std_logic;
    m_axis_tvalid: out std_logic;

--AXIS slave in eindpoint
    s_axis_tvalid: in std_logic;
    s_axis_tlast : in std_logic;
    s_axis_tready: out std_logic;
    s_axis_tdata : in STD_LOGIC_VECTOR(DATA_WIDTH-1 downto 0)
  );
end s_axis_to_w_fifo;

------------------------------------------------------------------------------------------------------------
architecture rtl of s_axis_to_w_fifo is

  signal full_reg   : std_logic;
  signal fifo_wr    : std_logic;
  signal full_sig   : std_logic;
  signal pending_wr : std_logic;
  signal data_reg   : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal tx_data    : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal tlast_reg  : std_logic;
  signal tready     : std_logic;
  signal tx_req     : std_logic;
  signal tx_grant   : std_logic;
  signal usb_oe     : std_logic;
  signal usb_oe_3s  : std_logic;
  signal tx_full    : std_logic;
  signal rx_data    : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal rx_data_r  : std_logic_vector(DATA_WIDTH-1 downto 0);
  signal rdn        : std_logic;
  signal rd_pend      : std_logic;
  signal tvalid     : std_logic;
  ------------------------------------------------------------------------------------------------------------
begin

  iobufs : for i in 0 to DATA_WIDTH-1 generate
  begin
    iobuf_n : iobuf 
      port map(
        O => rx_data(i),
        IO => dio(i),
        I => tx_data(i),
        T => usb_oe_3s
      );
  end generate;

  --dio <= tx_data when ft_oe = '0' else (others => 'Z');
  --rx_data <= dio;

  ------------------------------------------------------------------------------------------------------------

  p_tdata : process(clk)
  begin
    if rising_edge(clk) then

      if FLAGA = '1' and usb_oe = '1' and rdn = '0' then
        rx_data_r <= rx_data;
      end if;

      if reset = '1' then
        rd_pend <= '0';
      elsif FLAGA = '1' and usb_oe = '1' then
        if m_axis_tready = '0' and rdn = '0' then
          rd_pend <= '1';
        elsif m_axis_tready = '1' and tvalid = '1' then
          rd_pend <= '0';
        end if;
      end if;

      if m_axis_tready = '1' and FLAGA = '1' and usb_oe = '1' then
        if rdn = '0' then
          m_axis_tdata <= rx_data;
        else
          m_axis_tdata <= rx_data_r;
        end if;
      end if;
    end if;
  end process;
  ------------------------------------------------------------------------------------------------------------
  p_tvalid : process(clk)
  begin
    if rising_edge(clk) then
      if reset = '1' then
        tvalid <= '0';
      elsif tvalid = '0' or m_axis_tready = '1' then
         tvalid <= FLAGA and usb_oe and (not rdn or rd_pend);
      end if;
    end if;
  end process;

  m_axis_tvalid <= tvalid;
  ------------------------------------------------------------------------------------------------------------
  
  p_rdn : process(clk)
  begin
    if rising_edge(clk) then
      if reset = '1' then 
        rdn <= '1';
      elsif flaga = '1' and usb_oe = '1' and m_axis_tready = '1' then
        rdn <= '0';
      else
        rdn <= '1';
      end if;
    end if;
  end process;

  slrdn <= rdn;
  ------------------------------------------------------------------------------------------------------------
  bus_arbiter : process(clk)
  begin
    if rising_edge(clk) then
      if reset = '1' or tx_req = '0' or tx_full = '1' then
        usb_oe <= '1'; --always receiving unless
        usb_oe_3s <= '1'; --always receiving unless
        SLOEn <= '0';
        tx_grant <= '0';
      elsif (FLAGA = '0' or rdn = '1') and usb_oe = '1' and tx_req = '1' then
        usb_oe <= '0';
        usb_oe_3s <= '0';
        SLOEn <= '1'; 
      elsif usb_oe = '0' then
        tx_grant <= '1';
      end if;
    end if;
  end process;
  ------------------------------------------------------------------------------------------------------------

  SLWRn <= not fifo_wr or not FLAGB;--never allow a write when full

  --EP2 IN, EP8 OUT
  FIFOADDR0 <= usb_oe;
  FIFOADDR1 <= usb_oe;

  tx_full  <= not FLAGB;

  full_sig <= tx_grant and not tx_full;
  proc_delay: process (clk) is
  begin
    if rising_edge(clk) then
      full_reg <= full_sig;

      if reset = '1' then
        fifo_wr <= '0';
      elsif full_reg = '1' and full_sig = '1' then
        fifo_wr <= s_axis_tvalid;
      elsif pending_wr = '1' and full_sig = '1' then
        fifo_wr <= '1';
      else
        fifo_wr <= '0';
      end if;

      tlast_reg <= '0';
      if s_axis_tvalid = '1' and tready = '1' then
        data_reg <= s_axis_tdata;
        tlast_reg <= s_axis_tlast;
      end if;

      if reset = '1' then
        pending_wr <= '0';
      elsif pending_wr = '0' and full_sig = '0' and (s_axis_tvalid = '1' or fifo_wr = '1') then
        pending_wr <= '1';
      elsif pending_wr = '1' and fifo_wr = '1' then
        pending_wr <= '0';
      end if;

      if reset = '1' then
        tready <= '1';
      elsif tready = '1' and s_axis_tvalid = '1' then
        if (full_sig = '1' and full_reg = '0' and (fifo_wr = '1' or pending_wr = '1')) or
           (full_sig = '0' and full_reg = '1' and fifo_wr = '1' and pending_wr = '0') or
           (full_sig = '0' and full_reg = '0' and fifo_wr = '0' and pending_wr = '1')
        then
          tready <= '0';
        end if;
      elsif tready = '0' and fifo_wr = '1' then
        tready <= '1';
      end if;

      pktend  <= '1';
      if reset = '1' then
        tx_data <= X"CDCD";
      elsif pending_wr = '1' and fifo_wr = '1' then
        tx_data <=  data_reg;
        pktend  <=  not tlast_reg; 
      elsif pending_wr = '0' and s_axis_tvalid = '1' and ( fifo_wr = '0' or full_sig = '1' ) then
        tx_data <=  s_axis_tdata;
        pktend  <=  not s_axis_tlast;
      end if;
    end if;
  end process;
  s_axis_tready <= tready;

  tx_req <= s_axis_tvalid or fifo_wr or pending_wr;

end rtl;
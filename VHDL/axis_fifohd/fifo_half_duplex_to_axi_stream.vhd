library IEEE;
  use IEEE.STD_LOGIC_1164.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
  use IEEE.NUMERIC_STD.all;
  use ieee.std_logic_unsigned.all;

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
    tx_grant: in std_logic; --signals a wr grant
    tx_data : out STD_LOGIC_VECTOR(DATA_WIDTH-1 downto 0);
    -- FIFO control
    tx_full : in  STD_LOGIC; -- fifo full
    tx_req : out STD_LOGIC;
    tx_wr : out STD_LOGIC;
    pktend: out std_logic;

    s_axis_tvalid: in std_logic;
    --s_axis_tlast : in std_logic;
    s_axis_tready: out std_logic;
    s_axis_tdata : in STD_LOGIC_VECTOR(DATA_WIDTH-1 downto 0)
  );
end s_axis_to_w_fifo;

------------------------------------------------------------------------------------------------------------
architecture rtl of s_axis_to_w_fifo is

  signal full_reg: std_logic;
  signal fifo_wr: std_logic;
  signal full_sig: std_logic;
  signal pending_wr: std_logic;
  signal data_reg : std_logic_vector(15 downto 0);
  signal tready: std_logic;

  -- logic analyzer
  attribute mark_debug : string;
  attribute keep : string;
  attribute mark_debug of tx_full    : signal is "true";
  attribute mark_debug of tx_req      : signal is "true";
  attribute mark_debug of tx_wr       : signal is "true";
  attribute mark_debug of tx_grant    : signal is "true";
  attribute mark_debug of tx_data     : signal is "true";

begin
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

      if s_axis_tvalid = '1' and tready = '1' then
        data_reg <= s_axis_tdata;
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

      if reset = '1' then
        tx_data <= X"CDCD";
      elsif pending_wr = '1' and fifo_wr = '1' then
        tx_data <=  data_reg;
      elsif pending_wr = '0' and s_axis_tvalid = '1' and ( fifo_wr = '0' or full_sig = '1' ) then
        tx_data <=  s_axis_tdata;
      end if;
    end if;
  end process;
  s_axis_tready <= tready;
  tx_wr <= fifo_wr;
  tx_req <= s_axis_tvalid or fifo_wr or pending_wr;
  pktend <= '1'; --when s_axis_tlast = '1' and tx_full = '0' and tx_grant = '1' else '1';

end rtl;
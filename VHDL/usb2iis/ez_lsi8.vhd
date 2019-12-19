library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

library UNISIM;
  use UNISIM.VComponents.all;

entity ez_lsi8 is
  port (
    clk: in  std_logic;
    rst: in  std_logic;
--ez_ifc
    lsi8_rdnck  : in std_logic;
    lsi8_en     : in std_logic;
    lsi8_dio    : inout std_logic_vector(7 downto 0);
--read port
    lsi_rd_data : in std_logic_vector(31 downto 0);
    lsi_rd_addr : out std_logic_vector(7 downto 0);
    lsi_rd      : out std_logic;
    lsi_rd_done : out std_logic;
--write port
    lsi_wr_data : out std_logic_vector(31 downto 0);
    lsi_wr_addr : out std_logic_vector(7 downto 0);
    lsi_wr      : out std_logic
  );
end entity;

architecture rtl of ez_lsi8 is

  signal lsi8_en_i      : std_logic;
  signal lsi8_ck_i      : std_logic;
  type   t_lsi8_state is (gs_idle,gs_rd,gs_wr);
  signal lsi8_state     : t_lsi8_state;
  signal lsi8_do        : std_logic_vector(7 downto 0);
  signal lsi8_di        : std_logic_vector(7 downto 0);
  signal lsi8_oe        : std_logic;
begin

p_gpio: process(clk)
    variable bytenr : natural range 0 to 3 := 0;
begin
  if rising_edge(clk)then
    lsi8_en_i <= lsi8_en;
    lsi8_ck_i <= lsi8_rdnck;
    lsi_rd      <= '0';
    lsi_rd_done <= '0';
    if rst = '1' then
      lsi_rd_addr <= (others => '0');
      lsi_wr_addr <= (others => '0');
      lsi8_state  <= gs_idle;
      lsi8_do     <= X"CD";
      lsi8_oe     <= '1';
    elsif lsi8_state  = gs_idle and lsi8_en = '1' and lsi8_en_i = '0' then
      if lsi8_ck_i = '0' then
        lsi_rd      <= '1';
        lsi_rd_addr <= lsi8_di;
        lsi8_state  <= gs_rd;
      else
        lsi_wr_addr <= lsi8_di;
        lsi8_state  <= gs_wr;
      end if;
    elsif lsi8_state  /= gs_idle and lsi8_en = '0' and lsi8_en_i = '1' then
      if lsi8_state = gs_rd then
        lsi_rd_done <= '1';
      end if;
      lsi8_state  <= gs_idle;
    end if;

    if lsi8_state  = gs_idle then
      lsi8_oe     <= '1';
    elsif lsi8_state  = gs_rd then
      if lsi8_ck_i = '0' and lsi8_rdnck = '1' then
        lsi8_oe     <= '0';
      elsif lsi8_en = '0' and lsi8_en_i = '1' then
        lsi8_oe     <= '1';
      end if;
    end if;

    if rst = '1' then
      bytenr := 0;
    elsif lsi8_en /= lsi8_en_i then
      bytenr := 0;
    else
      if lsi8_state  = gs_rd then
        if (bytenr = 0 and  lsi8_ck_i = '1' and lsi8_rdnck = '0') or (bytenr /= 0 and lsi8_ck_i /= lsi8_rdnck) then
          bytenr := bytenr +1;
        end if;
      elsif lsi8_state  = gs_wr then
        if lsi8_ck_i /= lsi8_rdnck then
          bytenr := bytenr+1;
        end if;
      end if;
    end if;

    if lsi8_state  = gs_wr and lsi8_en = '0' and lsi8_en_i = '1' then
      lsi_wr <= '1';
    else
      lsi_wr <= '0';
    end if;

    if lsi8_state = gs_rd then
      case(bytenr) is
        when 0 => lsi8_do <= lsi_rd_data( 7 downto  0);
        when 1 => lsi8_do <= lsi_rd_data(15 downto  8);
        when 2 => lsi8_do <= lsi_rd_data(23 downto 16);
        when 3 => lsi8_do <= lsi_rd_data(31 downto 24);
      end case;
    elsif lsi8_state = gs_wr then
      if lsi8_en = '0' and lsi8_en_i = '1' then
        lsi_wr_data(31 downto 24) <= lsi8_di;
      else
        case(bytenr) is
          when 0 => lsi_wr_data( 7 downto  0) <= lsi8_di;
          when 1 => lsi_wr_data(15 downto  8) <= lsi8_di;
          when 2 => lsi_wr_data(23 downto 16) <= lsi8_di;
          when others => null ;
        end case;
      end if;
    end if;
  end if;
end process;

lsi8_iobufs : for i in 0 to 7 generate
begin
  iobuf_n : iobuf
    port map(
      O => lsi8_di(i),
      IO => lsi8_dio(i),
      I => lsi8_do(i),
      T => lsi8_oe
    );
end generate;

end architecture;

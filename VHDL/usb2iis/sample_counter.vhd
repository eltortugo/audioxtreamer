library IEEE;
  use IEEE.std_logic_1164.all;
  use ieee.std_logic_unsigned.all;
  use ieee.std_logic_arith.all;

entity sample_counter is
  generic( trig_freq : natural );
  port (
    clk: in  std_logic;
    rst: in  std_logic;

    f256 : in std_logic;
    trig : in std_logic; --sof
    spmf : out std_logic_vector(31 downto 0)
  );
end entity;

architecture rtl of sample_counter is
  signal f256_ctr: std_logic_vector(16 downto 0);  --will result in 4.13 format

  signal gate    : std_logic;
  signal gate_r  : std_logic_vector(3 downto 0);
  signal trig_div: natural range 0 to trig_freq-1;
  signal c_rst   : std_logic; 

  --attribute ASYNC_REG : string;
  --attribute ASYNC_REG of w_clk_r  : signal is "TRUE";
begin

  p_counter: process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        spmf <= (others=> '0');
        gate_r <= (others=> '0');
      else
        gate_r <=  gate_r(2 downto 0) & gate;
        if gate_r(3) = '1' and gate_r(2) = '0' then
          spmf <= x"000" & f256_ctr & "000";
        end if;
      end if;
    end if;
  end process;

  p_gate: process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' or c_rst = '1' then
        trig_div <= 0;
      elsif trig = '1' then
        trig_div <= trig_div+1;
      end if;

      if rst = '1' then
        gate <= '0';
      elsif c_rst = '1' then
        gate <= not gate;
      end if;
    end if;
  end process;

  c_rst <= '1' when trig = '1' and trig_div = trig_freq-1 else '0';

  p_f256 : process(f256,gate_r,c_rst)
  begin
    if c_rst = '1' and gate_r(0) = '0' then
      f256_ctr <= (others=> '0');
    elsif rising_edge(f256) and gate_r(0) = '1' then
      f256_ctr <= f256_ctr + 1;
    end if;
  end process;
end architecture;
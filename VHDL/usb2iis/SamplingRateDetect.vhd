library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;
  use ieee.std_logic_unsigned.all;

  use work.common_types.all;

entity SamplingRateDetect is
  port (
    clk: in  std_logic;
    rst: in  std_logic;
    word_clk: in  std_logic;
    sr_count: out slv_32
  );
end entity;

architecture rtl of SamplingRateDetect is
  signal edge_det : std_logic_vector(1 downto 0);
  attribute ASYNC_REG : string;
  attribute ASYNC_REG of edge_det : signal is "TRUE";
  
  constant gate_size : natural := 4800000; --100ms
  signal c100ms : natural range 0 to gate_size-1 := 0;
  signal pre_count : slv_16 := (others => '0');
  signal post_count : slv_16 := (others => '0');
  signal edge_count, edge_reg, reminder_reg : slv_16 := (others => '0');
  signal post_active : std_logic := '0';
  
begin
  
  ------------------------------------------------------------------------------------------------------------

  --sampling_rate_counter : process(usb_clk)
  --variable count : slv_16 := (others => '0');
  --variable edge : std_logic_vector(1 downto 0);
  --begin
  --  if rising_edge(usb_clk) then
  --    edge := edge(0) & sd_word_clk;
  --    if ft_reset = '1' or (edge(0) = '1' and edge(1) = '0') then
  --      sr_count <= count;
  --    count := (others => '0');
  --    else
  --      count := count + 1;
  --    end if;
  --  end if;
  --end process;

sampling_rate_counter : process(clk)
begin
  if rising_edge(clk) then
    edge_det <= edge_det(0) & word_clk;
    if rst = '1' or c100ms = gate_size-1 then
      c100ms <= 0;
      edge_reg <= edge_count;
      reminder_reg <= pre_count + post_count;
      pre_count <= (others => '0');
      post_count <= (others => '0');
      edge_count <= (others => '0');
      post_active <= '0';
    else
      c100ms <=  c100ms + 1;

      if post_active = '0' then 
        pre_count <= pre_count + 1;
      else
        post_count <= post_count + 1;
      end if;

      if (edge_det(0) = '1' and edge_det(1) = '0') then
        if post_active = '1' then 
          edge_count <= edge_count+1;
          post_count <= (others => '0');
        else
          post_active <= '1';
        end if;
      end if;
    end if;
  end if;
  sr_count <= reminder_reg & edge_reg;
end process;
end architecture;
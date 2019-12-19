library ieee;

  use ieee.std_logic_1164.all;
  use IEEE.numeric_std.all;
  use ieee.std_logic_unsigned.all;
  use work.common_types.all;


entity sampling_rate_detect is
  generic ( gate_length : positive := 2400000 );
  port(
    clk        : in std_logic;
    rst        : in std_logic;
    pcm_clk   : in std_logic;
    sr_count   : out slv_16
  );
end sampling_rate_detect;

architecture behavioral of sampling_rate_detect is
  signal run : std_logic := '0';
  signal gate_counter_i   : natural range 0 to gate_length := 0;
  signal sample_counter_i : slv_16 := (others => '0');
  signal done_i           : std_logic := '0';
  signal t0_i             : std_logic := '0';
  signal t_i              : std_logic := '0';
  signal ref_clk_div      : std_logic := '0';
  signal done_r : std_logic_vector(2 downto 0) := "000";
  signal rearm_r: std_logic_vector(2 downto 0) := "000";
  signal word_cke: std_logic := '0';
  signal pcm_128 : std_logic := '0';

  attribute ASYNC_REG : string;
  attribute ASYNC_REG of done_r  : signal is "TRUE";
  attribute ASYNC_REG of rearm_r : signal is "TRUE";


begin
  
  p_word_clk : process (pcm_clk)
    variable count : std_logic_vector(7 downto 0) :=  (others => '0');
  begin
    if rising_edge(pcm_clk) then
      count := count +1;
      word_cke <= '0';
      if count = X"7F" then
        word_cke <= '1';
      end if;
      pcm_128 <= count(7);
    end if;
  end process;

  p_sampl_counter : process(pcm_clk,run,word_cke)
  begin
    if run = '0' then
      sample_counter_i <= (others => '0');
    elsif rising_edge(pcm_clk) and word_cke = '1' then
      if t_i ='1' then
        sample_counter_i <= sample_counter_i+1;
      end if;
    end if;

    if rising_edge(pcm_clk) and word_cke = '1' then
      t_i <= t0_i;
      done_i <= (not t0_i) and run;
    end if;
  end process;


  p_gate_counter : process(clk,ref_clk_div) is
  begin
    if rising_edge(clk) then
      ref_clk_div <= not ref_clk_div; --generate the count en signal
    end if;

    if rising_edge(clk)  and ref_clk_div = '1'then
        if run = '0' then
          t0_i <= '0';
          gate_counter_i <= 0;
        elsif gate_counter_i = gate_length then
          t0_i <= '0';
        else
          t0_i <= '1';
          gate_counter_i <= gate_counter_i+1;
        end if;
      end if;
  end process;


  p_state: process(clk) is
  begin
    if rising_edge(clk) then
      if rst = '1' then
        run <= '0';
        done_r <= "000";
        rearm_r<= "000";
      else
        done_r <= done_r(1 downto 0) & done_i; --cdc + edge detect
        rearm_r<= rearm_R(1 downto 0) & pcm_128;

        if run = '0' and rearm_r (2 downto 1) = "10" then --prepare in the falling edge
          run <= '1';
        elsif run = '1' and done_r(2 downto 1) = "01" then --rising edge
          sr_count <= sample_counter_i;
          run <= '0';
        end if;
      end if;
    end if;
  end process;

end architecture behavioral;

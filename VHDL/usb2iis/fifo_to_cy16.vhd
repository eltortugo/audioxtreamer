library IEEE;
  use IEEE.STD_LOGIC_1164.all;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
  use IEEE.NUMERIC_STD.all;
  use ieee.std_logic_unsigned.all;

  use work.common_types.all;

------------------------------------------------------------------------------------------------------------

entity fifo_to_m_axis is
  generic(
    max_sdi_lines  : positive := 16
  );
  port (
    clk : in STD_LOGIC;
    reset : in STD_LOGIC;

    m_axis_tvalid: out std_logic;
    m_axis_tlast : out std_logic;
    m_axis_tready: in std_logic;
    m_axis_tdata : out STD_LOGIC_VECTOR(15 downto 0);

    nr_inputs: in std_logic_vector(3 downto 0);
    sof_int  : in std_logic;

    data_in : in std_logic_vector(15 downto 0);
    data_addr : out natural;

    in_fifo_empty: in std_logic;
    in_fifo_rd   : out std_logic;
    in_fifo_data : in slv24_array(0 to (max_sdi_lines*2)-1)
  );
end fifo_to_m_axis;

architecture Behavioral of fifo_to_m_axis is

  type tx_state_type is (header, audio);
  signal tx_state : tx_state_type := header;
  
  signal fifo_empty_r : std_logic;

  signal word_counter: natural range 0 to 512 ;
  signal sample_complete : std_logic;

  signal nr_ins : natural range 0 to max_sdi_lines;
  signal in_fifo_index: natural range 0 to (max_sdi_lines*2)-1;
  signal fifo_rd   : std_logic;
  signal tvalid   : std_logic;
  ------------------------------------------------------------------------------------------------------------
  -- logic analyzer
  attribute mark_debug : string;
  attribute keep : string;
  attribute mark_debug of tx_state    : signal is "true";
  attribute mark_debug of in_fifo_rd  : signal is "true";
  attribute mark_debug of in_fifo_empty : signal is "true";

  ------------------------------------------------------------------------------------------------------------
begin

  nr_ins <= to_integer(unsigned(nr_inputs)) + 1;
  
  ------------------------------------------------------------------------------------------------------------

  tx_fsm: process (clk)
  begin
    if rising_edge(clk) then

      fifo_empty_r <= in_fifo_empty;

      if reset = '1' then
        tx_state <= header;
      else

        case tx_state is
          when header =>
            if word_counter = 9 then
              tx_state <= audio;
            end if;

          when audio =>
            if sample_complete = '1' then
              if in_fifo_empty = '1' or (word_counter + (nr_ins*3)) >= 512 then
                tx_state <= header;
              end if;
            end if;
        end case;
      end if;
    end if;
  end process;

  in_fifo_rd <= fifo_rd;

  ------------------------------------------------------------------------------------------------------------

  m_axis_tlast <= '1' when sample_complete = '1' and (in_fifo_empty = '1' or (word_counter + (nr_ins*3)) > 511) else '0';
  fifo_rd <= '1' when in_fifo_empty = '0' and
                  ( word_counter = 9 or
                    (word_counter = 10 and fifo_empty_r = '1') or
                    ( word_counter > 10 and
                     ((word_counter + (nr_ins*3)) < 512) and
                     (sample_complete = '1')
                    )
                  ) else '0';


  sample_complete <= '1' when (in_fifo_index/2 = nr_ins-1 and ((word_counter-10) mod 3) = 2) else '0';
  ------------------------------------------------------------------------------------------------------------
  tvalid_proc: process (clk)
  begin
    if rising_edge(clk) then
      if reset = '1' then 
        tvalid <= '0';
      else
        case tx_state is
          when header => 
            tvalid <= '1';
            if word_counter = 9 and in_fifo_empty = '1' and m_axis_tready = '1' then
              tvalid <= '0'; --deassertion only when tready
            end if;
          when audio =>
            if tvalid = '0' and word_counter > 9 and fifo_rd = '1' then
              tvalid <= '1';
            --elsif sample_complete = '1' then --are we at the last word of the last channel
            --  if m_axis_tready = '1' then
            --    if  ((word_counter + (nr_ins*3)) < 512) and in_fifo_empty = '0' then
            --      tvalid <= '1';
            --    else
            --      tvalid <= '0';
            --    end if;
            --  end if;
            end if;
        end case;
      end if;
    end if;
  end process;

  m_axis_tvalid <= tvalid;

  ------------------------------------------------------------------------------------------------------------
  p_word_counter : process (clk)
  begin
    if rising_edge(clk) then
      if reset = '1' then
        word_counter <= 0;
      else
        case tx_state is
          when header  =>
            if m_axis_tready = '1' then
              word_counter <= word_counter +1;
            end if;
          when audio =>
            if sample_complete = '1' then
              if in_fifo_empty = '1' or (word_counter + (nr_ins*3)) > 511 then
                word_counter <= 0;
              elsif in_fifo_empty = '0' and tvalid = '1' and m_axis_tready = '1' and word_counter < 511 then
                word_counter <= word_counter +1;
              end if;
            elsif tvalid = '1' and m_axis_tready = '1' and word_counter < 511 then
              word_counter <= word_counter +1;
            end if;
        end case;
      end if;
    end if;
  end process;
------------------------------------------------------------------------------------------------------------
  data_addr <= word_counter;
------------------------------------------------------------------------------------------------------------
  tx_data_reg : process (clk, m_axis_tready, data_in, word_counter,in_fifo_data, in_fifo_index)
  begin
    --if rising_edge(clk) then
    --if m_axis_tready = '1' then
        case word_counter is
          when 0 => m_axis_tdata <= X"AAAA";
          when 1 => m_axis_tdata <= X"5555";
          when 2 to 9 => m_axis_tdata <= data_in;
          when others =>
            case ((word_counter - 10) mod 3) is
              when 0 => 
                m_axis_tdata <= in_fifo_data(in_fifo_index)(15 downto 0);
              when 1 =>
                m_axis_tdata <= in_fifo_data(in_fifo_index+1)(7 downto 0) & in_fifo_data(in_fifo_index)(23 downto 16);
              when 2 => 
                m_axis_tdata <= in_fifo_data(in_fifo_index+1)(23 downto 8);
              when others => 
                m_axis_tdata <= X"CACA";
            end case;
        end case;
    --end if;
    --end if;
  end process;
  ------------------------------------------------------------------------------------------------------------
  fifo_index : process (clk)
  begin
    if rising_edge(clk) then
      if tx_state = audio then 
        if tvalid = '1' and m_axis_tready = '1' and  ((word_counter-10) mod 3) = 2 then
          if in_fifo_index/2 = nr_ins-1 then
            in_fifo_index <= 0;
          else
            in_fifo_index <= in_fifo_index+2;
          end if;
        end if;
      else
        in_fifo_index <= 0;
      end if;
    end if;
  end process;
  ------------------------------------------------------------------------------------------------------------
end architecture;
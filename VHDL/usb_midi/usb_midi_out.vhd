------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------
-- AXI stream like unit that receives usb-midi packets and serializes them into the tx uart
-- It also uses removes the status byte when appropiate (running status)to reduce overhead.


library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.common_types.all;


entity usb_midi_out is
  port (
    clk: in  std_logic;
    rst: in  std_logic;

    --usb midi packets input
    s_axis_tdata  : in  std_logic_vector( 31 downto 0);
    s_axis_tvalid : in  std_logic;
    s_axis_tready : out std_logic;

    --midi bytes output
    m_axis_tdata  : out std_logic_vector(7 downto 0);
    m_axis_tvalid : out std_logic;
    m_axis_tready : in  std_logic
  );
end entity;

architecture rtl of usb_midi_out is
signal t_valid_i : std_logic;
type t_state is ( idle, decode, b2, b3 );
signal state : t_state;
signal code : std_logic_vector(3 downto 0);
signal byt1 : std_logic_vector(7 downto 0);
signal byt2 : std_logic_vector(7 downto 0);
signal byt3 : std_logic_vector(7 downto 0);
signal run_status: slv_8;
type t_array_of_codes is array (positive range 2 to 15) of positive range 1 to 3;
constant code_len : t_array_of_codes := (
   2 => 2,
   3 => 3,
   4 => 3,
   5 => 1,
   6 => 2,
   7 => 3,
   8 => 3,
   9 => 3,
  10 => 3,
  11 => 3,
  12 => 2,
  13 => 2,
  14 => 3,
  15 => 1
);

begin
  m_axis_tvalid <= t_valid_i;
  p_fsm: process (clk)
  begin 
    if rising_edge(clk) then
      if rst = '1' then
        state <= idle;
        s_axis_tready <= '1';
        t_valid_i     <= '0';
      else

        if t_valid_i = '1' and m_axis_tready = '1' then 
          t_valid_i <= '0';
        end if;

        case state is
          when idle =>
            if s_axis_tvalid = '1' then
              code <= s_axis_tdata( 3 downto  0);
              byt1 <= s_axis_tdata(15 downto  8);
              byt2 <= s_axis_tdata(23 downto 16);
              byt3 <= s_axis_tdata(31 downto 24);
              state <= decode;
              s_axis_tready <= '0';
            end if;
          when decode =>
            if t_valid_i = '0' or m_axis_tready = '1' then
              t_valid_i <= '1';
              case to_integer(unsigned(code)) is
                when 8 to 14 =>
                  if run_status = byt1 then --write byte 2 and 3
                    m_axis_tdata  <= byt2;
                    if code_len(to_integer(unsigned(code))) = 3 then
                      state <= b3;
                    else
                      state <= idle;
                      s_axis_tready <= '1';
                    end if;
                  else
                    run_status    <= byt1; --save the status to compare with the next packets
                    m_axis_tdata  <= byt1;
                    state <= b2;
                  end if;
                when 2 to 7 | 15 => --these invalidate the running status
                  run_status <= x"00";
                  t_valid_i     <= '1';
                  m_axis_tdata  <= byt1;
                  if code = x"5" or code = x"F" then
                    state <= idle;
                    s_axis_tready <= '1';
                  else
                    state <= b2;
                  end if;
                when others => --invalid code
                  run_status <= x"00";
                  state <= idle;
                  s_axis_tready <= '1';
              end case;
            end if;
          when b2 =>
            if t_valid_i = '0' or m_axis_tready = '1' then
              t_valid_i     <= '1';
              m_axis_tdata  <= byt2;
              if code_len(to_integer(unsigned(code))) = 3 then
                state <= b3;
              else
                state <= idle;
                s_axis_tready <= '1';
              end if;
            end if;
          when b3 =>
            if t_valid_i = '0' or m_axis_tready = '1' then
              t_valid_i     <= '1';
              m_axis_tdata  <= byt3;
              state <= idle;
              s_axis_tready <= '1';
            end if;
        end case;
      end if;
    end if;
  end process;

end architecture;
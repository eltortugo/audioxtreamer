------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

-- Uncomment the following library declaration if using
-- arithmetic functions with Signed or Unsigned values
use IEEE.NUMERIC_STD.ALL;
use ieee.std_logic_unsigned.all;

-- Uncomment the following library declaration if instantiating
-- any Xilinx leaf cells in this code.
--library UNISIM;
--use UNISIM.VComponents.all;

use work.simtools.all;

entity tb_top_audioxtreamer is
--  Port ( );
end entity;

architecture sim of tb_top_audioxtreamer is

constant nr_outputs     : natural := 18;
constant nr_inputs      : natural := 24;

constant dut_nr_outputs : natural := 24;
constant dut_nr_inputs  : natural := 24;

signal sd_out           : std_logic_vector(dut_nr_outputs/2 -1  downto 0);

type payload is array (natural range <>) of std_logic_vector(7 downto 0);
constant test_data: payload := 
( --sample 0
   X"A0", X"B0", X"C0" --ch01 
  ,X"A1", X"B1", X"C1" --ch02
  ,X"A2", X"B2", X"C2" --ch03
  ,X"A3", X"B3", X"C3" --ch04
  ,X"A4", X"B4", X"C4" --ch05
  ,X"A5", X"B5", X"C5" --ch06
  ,X"A6", X"B6", X"C6" --ch07
  ,X"A7", X"B7", X"C7" --ch08
  ,X"A8", X"B8", X"C8" --ch09
  ,X"A9", X"B9", X"C9" --ch10
  ,X"AA", X"BA", X"CA" --ch11
  ,X"AB", X"BB", X"CB" --ch12
  ,X"AC", X"BC", X"CC" --ch13
  ,X"AD", X"BD", X"CD" --ch14
  ,X"AE", X"BE", X"CE" --ch15
  ,X"AF", X"BF", X"CF" --ch16
  ,X"D0", X"E0", X"F0" --ch17
  ,X"D1", X"E1", X"F1" --ch18
--sample 1
  ,X"A0", X"B0", X"C0" --ch01 
  ,X"A1", X"B1", X"C1" --ch02
  ,X"A2", X"B2", X"C2" --ch03
  ,X"A3", X"B3", X"C3" --ch04
  ,X"A4", X"B4", X"C4" --ch05
  ,X"A5", X"B5", X"C5" --ch06
  ,X"A6", X"B6", X"C6" --ch07
  ,X"A7", X"B7", X"C7" --ch08
  ,X"A8", X"B8", X"C8" --ch09
  ,X"A9", X"B9", X"C9" --ch10
  ,X"AA", X"BA", X"CA" --ch11
  ,X"AB", X"BB", X"CB" --ch12
  ,X"AC", X"BC", X"CC" --ch13
  ,X"AD", X"BD", X"CD" --ch14
  ,X"AE", X"BE", X"CE" --ch15
  ,X"AF", X"BF", X"CF" --ch16
  ,X"D0", X"E0", X"F0" --ch17
  ,X"D1", X"E1", X"F1" --ch18
  ,X"aa", x"55", x"55", x"aa" --eof
  --,X"D2", X"E2", X"F2" --ch19
  --,X"D3", X"E3", X"F3" --ch20
  --,X"D4", X"E4", X"F4" --ch21
  --,X"D5", X"E5", X"F5" --ch22
  --,X"D6", X"E6", X"F6" --ch23
  --,X"D7", X"E7", X"F7" --ch24
  --,X"D8", X"E8", X"F8" --ch25
  --,X"D9", X"E9", X"F9" --ch26
  --,X"DA", X"EA", X"FA" --ch27
  --,X"DB", X"EB", X"FB" --ch28
  --,X"DC", X"EC", X"FC" --ch29
  --,X"DD", X"ED", X"FD" --ch30
  --,X"DE", X"EE", X"FE" --ch31
  --,X"DF", X"EF", X"FF" --ch32
);

constant payload_size   : natural := test_data'high + 1;--eof


signal usb_clk        : std_logic;
signal reset          : std_logic;

signal usb_dio        : std_logic_vector(15 downto 0);
signal SLOEn          : std_logic;
signal SLRDn          : std_logic;
signal wr_counter     : integer range 0 to payload_size -1;
signal out_data       : std_logic_vector(15 downto 0);

signal SLWRn          : std_logic;
signal tx_full        : std_logic;

signal flaga          : std_logic;
signal flagb          : std_logic;

signal yamaha_clk     : std_logic;
signal yamaha_word_clk: std_logic;

signal nr_writes      : natural;

signal lsi8_en        : std_logic;
signal lsi8_rdnck     : std_logic;
signal lsi8_dio       : std_logic_vector(7 downto 0);

signal counter : std_logic_vector (4 downto 0) := "00000";
begin

clk_gen(usb_clk, 48_000_000.0 );
clk_gen(yamaha_clk, 11_289_600.0 ); --44.1 khz
--clk_gen(yamaha_clk, 24_576_000.0 ); --96 khz

signal_gen(reset,usb_clk,'1', 10,'0', 1, '0', 1, false);

process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' then
      out_data <= test_data(1)& test_data(0);
      wr_counter <= 1;
    elsif SLRDn = '0' and SLOEn = '0'and flaga = '1' then
      if wr_counter = (payload_size/2-1) then
        wr_counter <= 0;
      else
        wr_counter <= wr_counter+1;
      end if;
      out_data <= test_data((wr_counter*2)+1)& test_data(wr_counter*2); -- simulate the flop clock/transfer delay
    end if;
  end if;
end process;

process (usb_clk)
begin
  if rising_edge(usb_clk) then
    if reset = '1' then
      counter <= "00000";
    else
      counter <= counter + 1;
    end if;
  end if;
end process;

flaga <= '0' when counter(4 downto 2) = "111" else '1';

process (yamaha_clk)
variable f256_count : std_logic_vector(7 downto 0);
begin
  if rising_edge(yamaha_clk) then
    if reset = '1' then
      f256_count := (others => '0');
    else
      f256_count := f256_count + X"01";
    end if;
  end if;
  yamaha_word_clk <= f256_count(7);
end process;

usb_dio <= out_data when SLOEn = '0' else (others => 'Z');

process (usb_clk, reset)
begin
  if rising_edge(usb_clk) then
    if reset = '1'then
      tx_full <= '0';
      nr_writes <= 0;
    else

      if SLWRn = '0' then
        nr_writes <= nr_writes +1;
      end if;

    end if;
  end if;
end process;

p_sim : process
begin
  lsi8_rdnck  <= '1';   --ready to write
  lsi8_dio    <= x"00";  --params addr
  lsi8_en     <= '0';
  wait until falling_edge(reset);
  wait for 1 ps;
  lsi8_dio    <= x"04"; --params addr
  wait until rising_edge(usb_clk);
  wait until rising_edge(usb_clk);
  wait until rising_edge(usb_clk);
  wait until rising_edge(usb_clk);
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_en     <= '1';
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_dio    <= x"12"; -- 18 outs
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_rdnck  <= '0';
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_dio    <= x"18";  -- 24 ins
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_rdnck  <= '1';
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_dio    <= x"10"; -- 16 bytes depth
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_rdnck  <= '0';
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_dio    <= x"00"; -- pad
  wait until rising_edge(usb_clk);
  wait for 1 ps;
  lsi8_en     <= '0';
  wait;
  
end process;


flagb <= not tx_full;

inst: entity work.top_audioxtreamer
  generic map( sdi_lines => dut_nr_inputs/2, sdo_lines => dut_nr_outputs/2 )--, bit_depth => 24 )
port map(

    areset => reset,

    cy_clk  => usb_clk,
    -- DATA IO
    dio    => usb_dio,

    FLAGA => flaga, -- repeat the message
    FLAGB => flagb,
    --PKTEND => pktend,
    SLRDn  => SLRDn,
    SLOEn  => SLOEn,
    SLWRn   => SLWRn,

    ain => sd_out(11 downto 0), --loopback
    aout => sd_out(11 downto 0),

    ymh_clk => yamaha_clk,
    ymh_word_clk => yamaha_word_clk,

   lsi8_en    => lsi8_en   ,
   lsi8_rdnck => lsi8_rdnck,
   lsi8_dio   => lsi8_dio  ,

   midi_rx => (others => '0')

    );
end architecture;

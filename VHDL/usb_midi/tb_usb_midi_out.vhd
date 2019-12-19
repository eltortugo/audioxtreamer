------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.common_types.all;
  use work.simtools.all;

entity tb_usb_midi_out is
end entity;

architecture sim of tb_usb_midi_out is
  signal clk: std_logic;
  signal rst: std_logic;

  --midi pkts source
  signal m_valid  : std_logic;
  signal m_ready  : std_logic;
  signal m_data   : std_logic_vector(31 downto 0);

  --midi bytes sink
  signal s_valid  : std_logic;
  signal s_ready  : std_logic;
  signal s_data   : std_logic_vector(7 downto 0);

  type usbpkt is array (natural range 0 to 3) of std_logic_vector(7 downto 0);

begin

  clk_gen(clk, 48_000_000.0);
  signal_gen(rst,clk,'1', 10,'0', 1, '0', 1, false);

  p_sim : process

    procedure tx_pkt( pkt : usbpkt) is
    begin
      loop
        wait until rising_edge(clk);
        if m_ready = '1' then
          m_data <= pkt(3)&pkt(2)&pkt(1)&pkt(0);
          m_valid  <= '1';
          exit;
        end if;
      end loop;
      wait until rising_edge(clk);
      m_valid  <= '0';
    end procedure;

    procedure expect( bytes: slv8_array) is
    begin
      for i in bytes'range loop
        if s_valid = '0' then
          wait until s_valid = '1';
        end if;
        assert s_data = bytes(i) report "FAIL: output 0x" & to_hstring( s_data ) & " /= expected 0x" & to_hstring(bytes(i)) severity error;
        echol( "s_data: 0x" & to_hstring(s_data));
        wait until rising_edge(clk);
        wait for 1 ps;
      end loop;
      assert s_valid = '0' report "FAIL: sender not done" severity error;
    end procedure;

  begin

    m_valid  <= '0';
    s_ready   <= '1';
    wait until falling_edge(rst);

    tx_pkt((X"02",x"f1",x"02",x"00"));
    expect(      (x"f1",x"02"));

    tx_pkt((X"03",x"f2",x"03",x"04"));
    expect(      (x"f2",x"03",x"04"));

    tx_pkt((X"04",x"f0",x"05",x"06"));
    expect(      (x"f0",x"05",x"06"));

    tx_pkt((X"05",x"f7",x"00",x"00"));
    expect(  (0=> x"f7"));

    tx_pkt((X"06",x"08",x"f7",x"00"));
    expect(      (x"08",x"f7"));

    tx_pkt((X"07",x"09",x"0a",x"f7"));
    expect(      (x"09",x"0a",x"f7"));

    tx_pkt((X"08",x"80",x"0b",x"0c"));
    expect(      (x"80",x"0b",x"0c"));
--test running status
    tx_pkt((X"08",x"80",x"0d",x"0e"));
    expect(            (x"0d",x"0e"));
--test that other midi channel is sent fully
    tx_pkt((X"08",x"81",x"0f",x"10"));
    expect(      (x"81",x"0f",x"10"));

--test that sys msg clears running status
    tx_pkt((X"02",x"f1",x"02",x"00"));
    expect(      (x"f1",x"02"));
    tx_pkt((X"08",x"81",x"0f",x"10"));
    expect(      (x"81",x"0f",x"10"));
--other chan msg
    tx_pkt((X"09",x"90",x"0b",x"0c"));
    expect(      (x"90",x"0b",x"0c"));
--poly after
    tx_pkt((X"0a",x"a0",x"00",x"01"));
    expect(      (x"a0",x"00",x"01"));
--CC
    tx_pkt((X"0b",x"b0",x"02",x"03"));
    expect(      (x"b0",x"02",x"03"));
--PC
    tx_pkt((X"0c",x"c0",x"02",x"03"));
    expect(      (x"c0",x"02"));
--running status on PC
    tx_pkt((X"0c",x"c0",x"04",x"00"));
    expect(       (0 => x"04"));
--Aftertouch
    tx_pkt((X"0d",x"d1",x"02",x"03"));
    expect(      (x"d1",x"02"));
--running status on aftertouch
    tx_pkt((X"0d",x"d1",x"04",x"00"));
    expect(       (0 => x"04"));
--pitch bend
    tx_pkt((X"0e",x"e2",x"0f",x"10"));
    expect(      (x"e2",x"0f",x"10"));
--runnin status on pitch bend
    tx_pkt((X"0e",x"e2",x"11",x"12"));
    expect(            (x"11",x"12"));





    wait until rising_edge(clk);

    wait;
    
  end process;



  inst_uut: entity work.usb_midi_out
    port map (
      clk           => clk,
      rst           => rst,
      s_axis_tdata  => m_data,
      s_axis_tvalid => m_valid,
      s_axis_tready => m_ready,
      m_axis_tdata  => s_data,
      m_axis_tvalid => s_valid,
      m_axis_tready => s_ready
    );


end architecture;

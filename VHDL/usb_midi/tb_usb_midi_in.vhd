------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------

library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.common_types.all;
  use work.simtools.all;

entity tb_usb_midi_in is
end entity;

architecture rtl of tb_usb_midi_in is
  signal clk: std_logic;
  signal rst: std_logic;
  --midi bytes source
  signal tx_valid  : std_logic;
  signal tx_ready  : std_logic;
  signal tx_data   : std_logic_vector(7 downto 0);

  --usb_midi pkts sink
  signal s_valid  : std_logic;
  signal s_ready  : std_logic;
  signal s_data   : std_logic_vector(31 downto 0);

  signal m_tvalid  : std_logic;
  signal m_tready  : std_logic;
  signal m_tdata   : std_logic_vector(31 downto 0);

  signal serial   : std_logic;--loopback

begin

  clk_gen(clk, 48_000_000.0);
  signal_gen(rst,clk,'1', 10,'0', 1, '0', 1, false);

  p_sim : process

  procedure tx_byte(byte : std_logic_vector(7 downto 0)) is
  begin
    tx_data <= byte;
    tx_valid  <= '1';
    if tx_ready = '0' then
      wait until tx_ready = '1';
    end if;
    wait until rising_edge(clk);
    wait for 1 ps;
  end procedure;

  procedure expect( pkt: std_logic_vector(31 downto 0)) is
  begin
    wait until s_valid = '1';
    assert s_data = pkt report "FAIL: pkt" severity error;
    echol( "sdata: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read" severity error;
  end procedure;

  procedure test_3b_w_running_status( code: std_logic_vector(3 downto 0); msg: std_logic_vector(7 downto 0)) is
  begin
    wait until rising_edge(clk);
    s_ready   <= '1';
    tx_byte(msg);
    tx_byte(X"10");
    tx_byte(X"20");
    tx_byte(X"30");
    tx_byte(X"40");
    tx_valid  <= '0';

    if s_valid = '0' then
      wait until s_valid = '1';
    end if;
    assert s_data = X"2010" & msg & x"0" & code report "FAIL: 3b msg" severity error;
    echol( "msg: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read" severity error;
    if s_valid = '0' then
      wait until s_valid = '1';
    end if;
    assert s_data = X"4030" & msg & x"0" & code  report "FAIL: 3b msg running status" severity error;
    echol( "msg: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read running status" severity error;
  end procedure;

  procedure test_2b_w_running_status(code: std_logic_vector(3 downto 0); msg: std_logic_vector(7 downto 0)) is
  begin
    wait until rising_edge(clk);
    s_ready   <= '1';
    tx_byte(msg);
    tx_byte(X"10");
    tx_byte(X"20");
    wait until rising_edge(clk);
    tx_valid  <= '0';
    wait until s_valid = '1';
    assert s_data = X"0010" & msg & x"0" & code report "FAIL: 2b msg" severity error;
    echol( "msg: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read" severity error;
    wait until s_valid = '1';
    assert s_data =  X"0020" & msg & x"0" & code report "FAIL: 2b msg running status" severity error;
    echol( "msg: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read running status" severity error;
  end procedure;
  
  procedure test_1b(code: std_logic_vector(3 downto 0); msg: std_logic_vector(7 downto 0)) is
  begin
    wait until rising_edge(clk);
    s_ready   <= '1';
    tx_byte(msg);
    wait until rising_edge(clk);
    tx_valid  <= '0';
    wait until s_valid = '1';
    assert s_data =  X"0000" & msg & x"0" & code  report "FAIL: msg" severity error;
    echol( "msg: 0x" & to_hstring( s_data ));
    wait until rising_edge(clk);
    wait for 1 ps;
    assert s_valid = '0' report "FAIL: valid didnt deassert after successful read" severity error;
  end procedure;

  procedure send_bytes( bytes: slv8_array ) is
  begin
    wait until rising_edge(clk);
    s_ready   <= '1';
    for i in bytes'range loop
      tx_byte(bytes(i));
    end loop;
    wait until rising_edge(clk);
    tx_valid  <= '0';
  end procedure;

begin
    tx_valid  <= '0';
    s_ready   <= '0';
    wait until falling_edge(rst);
    test_3b_w_running_status(X"8",X"81");--note off
    test_3b_w_running_status(X"9",X"90");--note on
    test_3b_w_running_status(X"A",X"A2");--Poly Aftertouch
    test_3b_w_running_status(X"B",X"B3");--Control Change
    test_2b_w_running_status(X"C",X"C4");--Program Change
    test_2b_w_running_status(X"D",X"D5");--Aftertouch
    test_3b_w_running_status(X"E",X"E6");--PitchBend Change
    test_2b_w_running_status(X"2",X"F1");--MTC
    test_3b_w_running_status(X"3",X"F2");--SPP
    test_2b_w_running_status(X"2",X"F3");--SongSelect

    test_1b(X"5",X"F6");
    test_1b(X"5",X"F8");
    test_1b(X"5",X"FA");
    test_1b(X"5",X"FB");
    test_1b(X"5",X"FE");
    test_1b(X"5",X"FF");

--sysx
    send_bytes((x"F0",x"F7"));
    expect(x"00f7f006");

    send_bytes((x"F0",x"01",x"F7"));
    expect(x"F701F007");

    send_bytes((x"F0",x"01",x"02",x"F7"));
    expect(x"0201F004");
    expect(x"0000F705");

    send_bytes((x"F0",x"01",x"02",x"03",x"F7"));
    expect(x"0201F004");
    expect(x"00F70306");

    send_bytes((x"F0",x"01",x"02",x"03",x"04",x"F7"));
    expect(x"0201F004");
    expect(x"F7040307");

    send_bytes((x"F0",x"01",x"02",x"03",x"04",x"05",x"F7"));
    expect(x"0201F004");
    expect(x"05040304");
    expect(x"0000F705");
--to see if we return to normal msgs
    test_3b_w_running_status(X"9",X"90");--note on



    --TODO: realtime messages between normal messages

    wait until rising_edge(clk);

    wait;
  end process;
  
  -- we test by entering the raw midi packets in the midi to usb block ,
  -- then into the midi io where it will be converted again back into midi,
  -- serial loopback back to midi back to usb midi
  

  
  i_uut: entity work.midi_io
    generic map (
      NR_CHANNELS => 1,
      BAUDRATE    => 1_600_000 --to speedup the sim a bit
    )
    port map (
      clk         => clk,
      reset       => rst,
      rx(1)       => serial,
      rx_data(1)  => s_data,
      rx_valid(1) => s_valid,
      rx_ready(1) => s_ready,
      rx_fifo_reset => rst,
      tx(1)       => serial,
      tx_data     => m_tdata,
      tx_ready    => m_tready,
      tx_valid    => m_tvalid
    );
  
  usb_midi_in_inst: entity work.usb_midi_in
    generic map (
      wire          => 0
    )
    port map (
      clk           => clk,
      rst           => rst,
      m_axis_tdata  => m_tdata,
      m_axis_tvalid => m_tvalid,
      m_axis_tready => m_tready,
      s_axis_tdata  => tx_data,
      s_axis_tvalid => tx_valid,
      s_axis_tready => tx_ready
    );




end architecture;


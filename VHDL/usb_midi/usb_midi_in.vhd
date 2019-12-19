------------------------------------------------------------------------------------------
-- Author:  Hector Soto, TurtleDesign

------------------------------------------------------------------------------------------
-- AXI stream like unit that receives raw midi bytes and packages them into usb-midi class compliant 32bit packets
-- It also decodes running status and passes realtime messages through in between sysex messages.


library IEEE;
  use IEEE.std_logic_1164.all;
  use IEEE.numeric_std.all;

  use work.common_types.all;


entity usb_midi_in is
  generic(wire : natural range 0 to 15);
  port (
    clk: in  std_logic;
    rst: in  std_logic;

    --usb_midi packets out
    m_axis_tdata  : out std_logic_vector(31 downto 0);
    m_axis_tvalid : out std_logic;
    m_axis_tready : in  std_logic;

    --midi packets input
    s_axis_tdata  : in  std_logic_vector( 7 downto 0);
    s_axis_tvalid : in  std_logic;
    s_axis_tready : out std_logic
  );
end entity;

architecture rtl of usb_midi_in is
  constant wire_nr : std_logic_vector := std_logic_vector(to_unsigned(wire,4));
  type t_state is ( status, byte1, byte2, sysx1,sysx2,sysx3,ready );
  signal state        : t_state := status;
  signal last_status  : std_logic_vector( 7 downto 0);
  signal usb_packet   : slv8_array(0 to 3);
  signal m_axis_tvalid_i : std_logic;
begin

  m_axis_tvalid <= m_axis_tvalid_i;


-- https://www.usb.org/sites/default/files/midi10.pdf p17
-- about sysex, weird beast where you have:
--  a) start normal  1: w4 f0 xx xx
--  b) continue       : w4 xx xx xx
--  c) end 1          : w5 f7 00 00
--  d) end 2          : w6 xx f7 00
--  e) end 3          : w7 xx xx f7
-- but also:
--  f) start and end 1: w6 f0 f7 00
--  g) start and end 2: w7 f0 xx f7
  
-- so when detecting f0 we just save the status and await sysx2 for a possible case f or await sysx3 for a case g.
-- If none of the previous then commit the packet and go to sysx1
-- then cycle through sysx1-sysx2-sysx3 where we could end at any time
-- in any case, we must commit a realtime message inmediately without forgetting the state we were at.

-- about running status 

  --Running Status is a data-thinning technique. It allows for the omision of status bytes if the current message to be transmitted
  --has the same status byte (i.e. the same command and MIDI channel) as the previous message.
  --It thus only applies to Channel (Voice and Mode) messages (0x8n - 0xEn).

  --System Real Time messages (0xF8 - 0xFF) are all single byte messages, and so running status is not applicable.
  --Also, they can be interleaved within other messages without affecting running status in any way.

  --System Common and System Exclusive messages (0xF0 - 0xF7), on the other hand, cancel any running status,
  --so any message following one of these messages must contain a status byte.
  --As with System Real Time messages, running status is not applicable to System Common and System Exclusive messages,
  --so these messages must always contain a status byte.

  p_fsm: process (clk)
  begin 
    if rising_edge(clk) then
      if rst = '1' then
        state <= status;
        usb_packet <= (others => (others => '0'));
      else
        case state is
          when status =>
            if s_axis_tvalid = '1' then
              case to_integer(unsigned(s_axis_tdata)) is
                when 16#80# to 16#EF# =>
                  last_status <= s_axis_tdata;
                  state <= byte1;
                  usb_packet(1) <= s_axis_tdata;
                  usb_packet(0) <= wire_nr & s_axis_tdata(7 downto 4);

                when 16#F1# to 16#F3# => --MidiTimeCode(2B), SongPosPtr(3B), SongSelect(2B)
                  last_status <= s_axis_tdata;
                  state <= byte1;
                  usb_packet(1) <= s_axis_tdata;
                  if s_axis_tdata = X"F2" then
                    usb_packet(0) <= wire_nr & X"3";
                  else
                    usb_packet(0) <= wire_nr & X"2";
                  end if;

                when 16#F4# | 16#F5# | 16#F9# | 16#FD# => null;--undefined ignore
                when 16#F6# | 16#F8# | 16#FA# | 16#FB# | 16#FC# | 16#FE# | 16#FF# => --single byte

                  usb_packet <= (wire_nr & X"5",s_axis_tdata,X"00",X"00");
                  state <= ready; --complete

                when 16#F0# => -- sysex start
                  last_status <= X"00"; --erase the running status
                  usb_packet(0) <= wire_nr & x"4"; --might change on case f ang g
                  usb_packet(1) <= s_axis_tdata;
                  state <= sysx2;

                when 16#F7# => null;--sysex end , we missed the start and the payload, too bad

                when others => --data under 0x80 , use the running status and move on
                  if   last_status(7 downto 4) = X"C"
                    or last_status(7 downto 4) = X"D"
                    or last_status             = X"F1"
                    or last_status             = X"F3" then --2 byte data msg

                    state <= ready; --complete
                    usb_packet(3) <= X"00";
                  else
                    state <= byte2;
                  end if;
                  usb_packet(2)  <= s_axis_tdata;
                  usb_packet(1)  <= last_status;
                  case to_integer(unsigned(last_status)) is
                    when 16#F1# | 16#F3#  => usb_packet(0) <= wire_nr & X"2";
                    when 16#F2#           => usb_packet(0) <= wire_nr & X"3";
                    when others           => usb_packet(0) <= wire_nr & last_status(7 downto 4);
                  end case;
              end case;
            end if;

          when byte1 =>
            if s_axis_tvalid = '1' then
              usb_packet(2)  <= s_axis_tdata;
              if   last_status(7 downto 4) = X"C"
                or last_status(7 downto 4) = X"D"
                or last_status             = X"F1" 
                or last_status             = X"F3" then --2 byte data msg
                state <= ready; --complete
                usb_packet(3) <= X"00";
              else
                state <= byte2;
              end if;
            end if;
          when byte2 =>
            if s_axis_tvalid = '1' then
              usb_packet(3)  <= s_axis_tdata;
              state <= ready; --complete
            end if;
          when sysx1 =>
            if s_axis_tvalid = '1' then
              usb_packet(1) <= s_axis_tdata;
              if s_axis_tdata = X"F7" then
                usb_packet(0)( 3 downto  0) <= x"5";
                usb_packet(2) <= X"00";
                usb_packet(3) <= X"00";
                state <= ready; --complete
              else
                state <= sysx2;
              end if;
            end if;
          when sysx2 =>
            if s_axis_tvalid = '1' then
              usb_packet(2) <= s_axis_tdata;
              if s_axis_tdata = X"F7" then
                usb_packet(0)( 3 downto  0) <= x"6";
                usb_packet(3) <= X"00";
                state <= ready; --complete
              else
                state <= sysx3;
              end if;
            end if;
          when sysx3 =>
            if s_axis_tvalid = '1' then
              if s_axis_tdata = X"F7" then --case g
                usb_packet(0)( 3 downto  0) <=  X"7";
              end if;
              usb_packet(3) <= s_axis_tdata;
              state <= ready; --complete
            end if;
          when ready =>
            if m_axis_tvalid_i = '0' or m_axis_tready = '1' then
              if usb_packet(0)(3 downto 0) = X"4" then --continue in sysex
                state <= sysx1;
              else
                state <= status;
              end if;
              m_axis_tdata <= usb_packet(3) & usb_packet(2) &usb_packet(1) & usb_packet(0);
            end if;
        end case;
      end if;
    end if;
  end process;

  -- we are always ready unless we are stalled and depend on the master ready
  s_axis_tready <= m_axis_tvalid_i and m_axis_tready when state = ready else '1';

  p_axis_tvalid_i: process(clk)
  begin
    if rising_edge(clk) then
      if rst = '1' then
        m_axis_tvalid_i <= '0';
      elsif state = ready and (m_axis_tvalid_i = '0' or m_axis_tready = '1') then --assert valid when ready and deassert only if the slave is ready
        m_axis_tvalid_i <= '1';
      elsif m_axis_tvalid_i = '1' and m_axis_tready = '1' then
        m_axis_tvalid_i <= '0';
      end if;
    end if;
  end process;


end architecture;
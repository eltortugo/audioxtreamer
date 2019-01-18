library IEEE;
  use IEEE.STD_LOGIC_1164.all;
  use IEEE.NUMERIC_STD.all;
  use ieee.std_logic_unsigned.all;


package simtools is
  function MAX(LEFT, RIGHT: INTEGER) return INTEGER;
  procedure clk_gen(signal clk : out std_logic; constant FREQ : real);
  procedure signal_gen(
    signal sig : out std_logic;
    signal clk : in std_logic;
    constant delay_val : std_logic;
    constant delay_clks: natural;
    constant on_val : std_logic;
    constant on_clks: natural;
    constant off_val : std_logic;
    constant off_clks: natural;
    constant repeat : boolean
  );
end package;

package body simtools is
function MAX(LEFT, RIGHT: INTEGER) return INTEGER is
begin
  if LEFT > RIGHT then return LEFT;
else return RIGHT;
end if;
end;

-- Procedure for clock generation
procedure clk_gen(signal clk : out std_logic; constant FREQ : real) is
  constant PERIOD    : time := 1 sec / FREQ;        -- Full period
  constant HIGH_TIME : time := PERIOD / 2;          -- High time
  constant LOW_TIME  : time := PERIOD - HIGH_TIME;  -- Low time; always >= HIGH_TIME
begin
  -- Check the arguments
  assert (HIGH_TIME /= 0 fs) report "clk_plain: High time is zero; time resolution to large for frequency" severity FAILURE;
  -- Generate a clock cycle
  loop
    clk <= '1';
    wait for HIGH_TIME;
    clk <= '0';
    wait for LOW_TIME;
  end loop;
end procedure; 

procedure signal_gen(
  signal sig : out std_logic;
  signal clk : in std_logic;
  constant delay_val : std_logic;
  constant delay_clks: natural;
  constant on_val : std_logic;
  constant on_clks: natural;
  constant off_val : std_logic;
  constant off_clks: natural;
  constant repeat : boolean
) is
  variable count : natural range 0 to MAX(MAX(delay_clks,on_clks),off_clks) := delay_clks;
  variable state : natural range 0 to 2 := 0;
begin
  if delay_clks > 0 then
    sig <= delay_val;
    state := 0;
    count := delay_clks;
  else
    sig <= on_val;
    state := 1;
    count := on_clks;
  end if;
  loop
    count := count -1;
    wait until rising_edge(clk);
    if count = 0 then
      case state is
        when 0 =>
          state := 1;
          sig <= on_val;
          if on_clks > 0 then
            count := on_clks;
          else
            wait;
          end if;
        when 1 =>
          sig <= off_val;
          state := 2;
          if off_clks > 0 then
            count := off_clks;
          else
            wait;
          end if;
        when 2 =>
          if repeat = true then
            state := 1;
            count := on_clks;
            sig <= on_val;
          else
            wait;
          end if;
      end case;
    end if;
  end loop;
end procedure;

end simtools;

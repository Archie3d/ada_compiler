with Ada.Text_IO;
use Ada.Text_IO;

procedure Caseerrors is

    type Day is (Mon, Tue, Wed, Thu, Fri, Sat, Sun);

    D : Day := Mon;
    N : Integer := 3;
    S : Integer range 1 .. 5 := 2;
    R : Float := 1.0;

begin
    -- Ada asks a case to account for every value, so leaving some out without
    -- an 'others' is an error rather than a case that quietly does nothing.
    case D is
        when Mon => null;
        when Tue => null;
    end case;

    case N is
        when 1 => null;
        when 2 => null;
    end case;

    -- No value may be covered twice, whether by a repeat or by an overlap.
    case D is
        when Mon    => null;
        when Mon    => null;
        when others => null;
    end case;

    case D is
        when Mon .. Wed => null;
        when Tue        => null;
        when others     => null;
    end case;

    -- A choice outside what the selector's subtype holds names nothing.
    case S is
        when 1 .. 5 => null;
        when 9      => null;
    end case;

    -- An empty range covers nothing either.
    case D is
        when Fri .. Mon => null;
        when others     => null;
    end case;

    -- 'others' stands for whatever is left, so nothing may follow it.
    case D is
        when others => null;
        when Mon    => null;
    end case;

    -- A case chooses between values, which a real number has no discrete
    -- supply of.
    case R is
        when 1.0    => null;
        when others => null;
    end case;

    -- A choice has to be settled at compile time.
    case N is
        when 1      => null;
        when N      => null;
        when others => null;
    end case;
end Caseerrors;

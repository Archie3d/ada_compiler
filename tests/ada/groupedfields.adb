with Ada.Text_IO; use Ada.Text_IO;
procedure GroupedFields is
    Calls : Integer := 0;
    function Next_Value return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next_Value;
    type Pair is record
        X, Y : Integer range 1 .. 10 := Next_Value;
        Left, Right : String (3 .. 4) := "ok";
    end record;
    A : Pair;
    B : Pair;
    type Kind is (Empty, Full);
    type Variant (Tag : Kind) is record
        case Tag is
            when Empty => null;
            when Full => X, Y : Integer range 1 .. 10 := Next_Value;
        end case;
    end record;
    E : Variant (Empty);
    V : Variant (Full);
    type Wrapper is record
        First, Second : Variant (Full);
    end record;
    W : Wrapper;
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    type Grids is record
        First, Second : Matrix (2 .. 3, 4 .. 5) := ((1, 2), (3, 4));
    end record;
    G : Grids;
begin
    if Calls /= 10 or Integer (A.X) + Integer (A.Y) /= 3 or Integer (B.X) + Integer (B.Y) /= 7
        or Integer (V.X) + Integer (V.Y) /= 11 or Integer (W.First.X) + Integer (W.First.Y) /= 15
        or Integer (W.Second.X) + Integer (W.Second.Y) /= 19
        or A.Left /= "ok" or A.Right /= "ok"
        or A.Left'First /= 3 or A.Right'Last /= 4
        or G.First (2, 4) /= 1 or G.Second (3, 5) /= 4
        or G.First'First (2) /= 4 or G.Second'Last (1) /= 3
    then
        raise Program_Error;
    end if;
    Put_Line ("grouped fields ok");
end GroupedFields;

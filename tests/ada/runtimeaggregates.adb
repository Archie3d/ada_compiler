with Ada.Text_IO; use Ada.Text_IO;
procedure RuntimeAggregates is
    type Vector is array (Integer range <>) of Integer;
    Calls : Integer := 0;
    Bound_Calls : Integer := 0;
    function Bound (N : Integer) return Integer is
    begin
        Bound_Calls := Bound_Calls + 1;
        return N;
    end Bound;
    function Next return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next;
    procedure Run (Low, High : Integer) is
        Values : Vector (Low .. High) := (1, 2, 3);
        Copy : Vector (Low .. High) := (1, others => 9);
        Fixed : Vector (1 .. 3) := (Bound (4) .. Bound (6) => 5);
    begin
        if Bound_Calls /= 2 or Fixed (1) /= 5 or Fixed (3) /= 5 then
            raise Program_Error;
        end if;
        if Values (Low) /= 1 or Values (High) /= 3 or Copy (Low + 1) /= 9 then
            raise Program_Error;
        end if;
        Values := (Values (High), Values (Low + 1), Values (Low));
        if Values (Low) /= 3 or Values (High) /= 1 then
            raise Program_Error;
        end if;
        Values := (1 => 11, 2 .. 3 => 12);
        if Values (Low) /= 11 or Values (High) /= 12 then
            raise Program_Error;
        end if;
        Values := (Low .. High => Next);
        if Values (Low) /= Calls - 2 or Values (High) /= Calls then
            raise Program_Error;
        end if;
    end Run;
    procedure Named (Size : Integer) is
        Values : Vector (1 .. Size) := (1 | 3 => 7, others => 8);
    begin
        if Values (1) /= 7 or Values (2) /= 8 or Values (3) /= 7 or Values (4) /= 8 then
            raise Program_Error;
        end if;
    end Named;
    procedure Empty (Low, High : Integer) is
        Values : Vector (Low .. High) := (Low .. High => Next);
    begin
        if Values'Length /= 0 then
            raise Program_Error;
        end if;
    end Empty;
begin
    Run (4, 6);
    Named (4);
    Empty (8, 3);
    if Calls /= 3 then
        raise Program_Error;
    end if;
    Put_Line ("positional, named, dynamic ranges, sliding, and null aggregates");
end RuntimeAggregates;

with Ada.Text_IO; use Ada.Text_IO;
procedure InferredAggregates is
    subtype Index is Integer range 5 .. 50;
    type Vector is array (Index range <>) of Integer;
    Calls : Integer := 0;
    Bound_Calls : Integer := 0;
    function Next return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next;
    function Bound (N : Integer) return Integer is
    begin
        Bound_Calls := Bound_Calls + 1;
        return N;
    end Bound;
    function Make return Vector is
    begin
        return (11, 12, 13);
    end Make;
    function Named return Vector is
    begin
        return (9 => 3, 7 .. 8 => 2);
    end Named;
    function Dynamic (Low, High : Integer) return Vector is
    begin
        return (Bound (Low) .. Bound (High) => Next);
    end Dynamic;
    procedure Verify (Value : Vector; Low, High : Integer) is
    begin
        if Value'First /= Low or Value'Last /= High then
            raise Program_Error;
        end if;
    end Verify;
    A : Vector := (1, 2, 3);
    B : Vector := (8 .. 10 => 4);
    C, D : Vector := (Next, Next);
    Text : String := ('a', 'b', 'c');
begin
    Verify (A, 5, 7);
    Verify (B, 8, 10);
    Verify (Make, 5, 7);
    Verify (Named, 7, 9);
    Verify ((6 => 1, 7 => 2), 6, 7);
    if C (5) /= 1 or C (6) /= 2 or D (5) /= 3 or D (6) /= 4 or Text /= "abc" then
        raise Program_Error;
    end if;
    declare
        E : Vector := Dynamic (12, 14);
        Empty : Vector := Dynamic (9, 6);
        F : Vector := (Bound (20) .. Bound (21) => Next);
    begin
        Verify (E, 12, 14);
        Verify (Empty, 9, 6);
        Verify (F, 20, 21);
        if E (12) /= 5 or E (14) /= 7 or Empty'Length /= 0 or Calls /= 9 or Bound_Calls /= 6 then
            raise Program_Error;
        end if;
    end;
    A := Named;
    if A (5) /= 2 or A (7) /= 3 then
        raise Program_Error;
    end if;
    Put_Line ("inferred bounds, returned aggregates, null ranges, and grouped evaluation");
end InferredAggregates;

with Ada.Text_IO; use Ada.Text_IO;
procedure ArrayIdentity is
    type Vector is array (Integer range <>) of Integer;
    type Other_Vector is array (Integer range <>) of Integer;
    subtype Pair is Vector (1 .. 2);
    A : Pair := (1, 2);
    B : Vector (5 .. 6) := A;
    Other : Other_Vector (1 .. 2) := (3, 4);
    function Pick (V : Vector) return Integer is
    begin
        return 1;
    end Pick;
    function Pick (V : Other_Vector) return Integer is
    begin
        return 2;
    end Pick;
    function Make return Vector is
    begin
        return (7, 8);
    end Make;
    function Make return Other_Vector is
    begin
        return (9, 10);
    end Make;
    type Text is array (Positive range <>) of Character;
    type Other_Text is array (Positive range <>) of Character;
    T : Text (4 .. 5) := "ab";
    U : Other_Text (1 .. 2) := "xy";
    function Text_Pick (V : Text) return Integer is
    begin
        return 3;
    end Text_Pick;
    function Text_Pick (V : Other_Text) return Integer is
    begin
        return 4;
    end Text_Pick;
    procedure Check_Text (V : Text := "ab") is
    begin
        if V /= "ab" or "ab" /= V then
            raise Program_Error;
        end if;
    end Check_Text;
    type Matrix is array (Integer range <>, Integer range <>) of Integer;
    M : Matrix (1 .. 2, 3 .. 4) := ((1, 2), (3, 4));
    N : Matrix (5 .. 6, 7 .. 8) := M;
    Size : Integer := 2;
    subtype Dynamic is Vector (1 .. Size);
    D : Dynamic := (1, 2);
begin
    if A /= B or Pick (A) /= 1 or Pick (Other) /= 2
        or Text_Pick (T) /= 3 or Text_Pick (U) /= 4 or M /= N
    then
        raise Program_Error;
    end if;
    A := Make;
    Other := Make;
    if A (1) /= 7 or Other (1) /= 9 then
        raise Program_Error;
    end if;
    A := Vector (Other);
    if A (1) /= 9 or A (2) /= 10 then
        raise Program_Error;
    end if;
    A := B (5 .. 6);
    D := A;
    if D /= A or D /= (1, 2) or (1, 2) /= D then
        raise Program_Error;
    end if;
    Check_Text;
    Check_Text ("ab");
    Check_Text ("a" & "b");
    Check_Text (Text'("ab"));
    if ("a" & "b") /= T or Text_Pick (T & "") /= 3 then
        raise Program_Error;
    end if;
    T := 'a' & "b";
    Check_Text (T);
    T := "a" & T (5 .. 5);
    Check_Text (T);
    Put_Line ("array identity ok");
end ArrayIdentity;

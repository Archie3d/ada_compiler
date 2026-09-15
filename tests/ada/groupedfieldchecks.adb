with Ada.Text_IO; use Ada.Text_IO;
procedure GroupedFieldChecks is
    type Pair is record
        X, Y : Integer range 1 .. 3 := 2;
        Left, Right : String (1 .. 2) := "ok";
    end record;
    P : Pair;
    Count : Integer := 0;
    Calls : Integer := 0;
    function Fail_Second return Integer is
    begin
        Calls := Calls + 1;
        if Calls = 2 then
            raise Constraint_Error;
        end if;
        return 1;
    end Fail_Second;
    function Wrong_Length return String is
    begin
        return "bad";
    end Wrong_Length;
    type Defaults is record
        X, Y, Z : Integer := Fail_Second;
    end record;
begin
    begin
        P.X := 4;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        P.Y := 4;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        P.Left := Wrong_Length;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        P.Right := Wrong_Length;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        declare
            D : Defaults;
        begin
            raise Program_Error;
        end;
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if Count /= 5 or Calls /= 2 or P.X /= 2 or P.Y /= 2
        or P.Left /= "ok" or P.Right /= "ok"
    then
        raise Program_Error;
    end if;
    Put_Line ("grouped field checks ok");
end GroupedFieldChecks;

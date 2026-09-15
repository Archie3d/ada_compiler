with Ada.Text_IO; use Ada.Text_IO;
procedure ScalarCopy is
    subtype Small is Integer range 1 .. 3;
    X : Small := 2;
    Count : Integer := 0;
    Entered : Boolean := False;
    procedure Set_Value (V : out Integer) is
    begin
        V := 9;
        if X /= 2 then
            raise Program_Error;
        end if;
    exception
        when Constraint_Error => raise Program_Error;
    end Set_Value;
    procedure Fail (V : in out Integer; W : out Integer) is
    begin
        V := 3;
        W := 3;
        raise Constraint_Error;
    end Fail;
    procedure Narrow (V : in out Small) is
    begin
        Entered := True;
    end Narrow;
    procedure Narrow_Out (V : out Small) is
    begin
        V := 3;
    end Narrow_Out;
    procedure Alias_Test (A, B : in out Integer) is
    begin
        A := 10;
        if B /= 2 then
            raise Program_Error;
        end if;
        B := 10;
    end Alias_Test;
    procedure Nested (V : in out Integer) is
        procedure Inner is
        begin
            V := V + 1;
        end Inner;
    begin
        Inner;
    end Nested;
    procedure Recover (V : out Integer) is
    begin
        V := 3;
        raise Constraint_Error;
    exception
        when Constraint_Error => V := 1;
    end Recover;
    procedure Early (V : out Integer) is
    begin
        V := 3;
        return;
    end Early;
    Y : Integer := 9;
    Z : Integer := 2;
    type Vector is array (1 .. 2) of Small;
    Values : Vector := (2, 2);
    Index_Calls : Integer := 0;
    function Index return Integer is
    begin
        Index_Calls := Index_Calls + 1;
        return 1;
    end Index;
    type Holder is record
        Value : Small;
    end record;
    H : Holder := (Value => 2);
    type Link is access Small;
    P : Link := new Small'(2);
    procedure Keep (V : out Link) is
    begin
        if V = null then
            raise Program_Error;
        end if;
    end Keep;
    procedure Lose (V : in out Link) is
    begin
        V := null;
        raise Constraint_Error;
    end Lose;
begin
    begin
        Set_Value (X);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Fail (X, Z);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if X /= 2 or Z /= 2 then
        raise Program_Error;
    end if;
    begin
        Narrow (Y);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if Entered or Y /= 9 then
        raise Program_Error;
    end if;
    Narrow_Out (Y);
    if Y /= 3 then
        raise Program_Error;
    end if;
    Alias_Test (Z, Z);
    if Z /= 10 then
        raise Program_Error;
    end if;
    Nested (X);
    if X /= 3 then
        raise Program_Error;
    end if;
    Recover (X);
    if X /= 1 then
        raise Program_Error;
    end if;
    Early (X);
    Nested (Values (Index));
    Nested (H.Value);
    Nested (P.all);
    Keep (P);
    begin
        Lose (P);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if X /= 3 or Values (1) /= 3 or Values (2) /= 2
        or H.Value /= 3 or P = null or P.all /= 3
        or Index_Calls /= 1 or Count /= 4
    then
        raise Program_Error;
    end if;
    Put_Line ("scalar copy ok");
end ScalarCopy;

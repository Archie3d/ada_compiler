with Ada.Text_IO; use Ada.Text_IO;
procedure ScalarCopyChecks is
    Limit : Integer := 3;
    subtype Small is Integer range 1 .. Limit;
    X : Small := 2;
    Count : Integer := 0;
    Entered : Boolean := False;
    procedure Set_Value (V : out Integer) is
    begin
        V := 4;
    end Set_Value;
    procedure Narrow (V : in out Small) is
    begin
        Entered := True;
    end Narrow;
    procedure Nested (V : in out Integer) is
    begin
        Set_Value (V);
        raise Constraint_Error;
    end Nested;
    procedure Recover_Copy (V : in out Small) is
    begin
        Set_Value (V);
    exception
        when Constraint_Error => V := 3;
    end Recover_Copy;
    procedure Pair (A, B : out Integer) is
    begin
        A := 3;
        B := 4;
    end Pair;
    Y : Integer := 4;
    Wide_Limit : Long_Integer := 4_294_967_299;
    subtype Wide_Small is Long_Integer range 4_294_967_297 .. Wide_Limit;
    Wide : Wide_Small := 4_294_967_298;
    procedure Set_Wide (V : out Long_Integer) is
    begin
        V := 8_589_934_594;
    end Set_Wide;
    type Color is (Red, Green, Blue);
    Last_Color : Color := Green;
    subtype Colors is Color range Red .. Last_Color;
    C : Colors := Green;
    procedure Set_Color (V : out Color) is
    begin
        V := Blue;
    end Set_Color;
    subtype Fraction is Float range 0.0 .. 1.0;
    F : Fraction := 0.5;
    procedure Set_Float (V : out Float) is
    begin
        V := 2.0;
    end Set_Float;
    procedure Recursive (Depth : Integer) is
        Bound : Integer := Depth;
        subtype Local is Integer range 0 .. Bound;
        V : Local := 0;
    begin
        if Depth > 1 then
            Recursive (Depth - 1);
        end if;
        begin
            Set_Value (V);
        exception
            when Constraint_Error => Count := Count + 1;
        end;
        if V /= 0 then
            raise Program_Error;
        end if;
    end Recursive;
begin
    Limit := 10;
    begin
        Set_Value (X);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Narrow (Y);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Nested (X);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Set_Wide (Wide);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Set_Color (C);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    begin
        Set_Float (F);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    Recursive (3);
    if X /= 2 or Y /= 4 or Entered or Wide /= 4_294_967_298
        or C /= Green or F /= 0.5 or Count /= 9
    then
        raise Program_Error;
    end if;
    Recover_Copy (X);
    if X /= 3 then
        raise Program_Error;
    end if;
    X := 2;
    Y := 0;
    begin
        -- Copy-back follows formal order, even with named associations.
        -- The successful first copy is retained when the second check fails.
        Pair (B => X, A => Y);
    exception
        when Constraint_Error => Count := Count + 1;
    end;
    if X /= 2 or Y /= 3 or Count /= 10 then
        raise Program_Error;
    end if;
    Put_Line ("scalar copy checks ok");
end ScalarCopyChecks;

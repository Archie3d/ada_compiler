with Ada.Text_IO;
with Ada.Numerics;
with Ada.Numerics.Elementary_Functions;
with Ada.Numerics.Long_Elementary_Functions;
with Ada.Numerics.Generic_Elementary_Functions;
procedure Basepower is
    use Ada.Numerics.Elementary_Functions;
    type Small is range 1 .. 10;
    subtype Smaller is Small range 2 .. 3;
    type Real is digits 12 range 1.0 .. 2.0;
    subtype Narrow is Real range 1.0 .. 1.5;
    package Math is new Ada.Numerics.Generic_Elementary_Functions (Narrow);
    package Base_Math is new Ada.Numerics.Generic_Elementary_Functions (Narrow'Base);
    type Color is (Red, Green, Blue);
    subtype One_Color is Color range Green .. Green;
    type Palette is array (Color'Base range Red .. Blue) of Integer;
    Colors : Palette := (1, 2, 3);
    Count : Integer := 0;
    C : One_Color'Base := Red;
    I : Smaller'Base := 100;
    X : Narrow'Base := Math.Sqrt (9.0);
    F : Float := 9.0 ** 0.5;
    N : Integer := 3;
    L : Long_Float;
    function Echo (Value : Narrow'Base) return Real'Base is
    begin
        return Value;
    end Echo;
begin
    if I /= 100 or X /= 3.0 or F /= 3.0 or Echo (4.0) /= 4.0
        or Smaller'Base'First /= Small'Base'First
        or Smaller'Base'Last /= Small'Base'Last
        or Smaller'Base'First >= 1 or Smaller'Base'Last <= 10
        or Narrow'Base'First >= 1.0 or Narrow'Base'Last <= 2.0 then
        raise Program_Error;
    end if;
    for Shade in One_Color'Base loop
        Count := Count + Colors (Shade);
    end loop;
    if Count /= 6 or not (100 in Smaller'Base) or C /= Red
        or One_Color'Base'First /= Red or One_Color'Base'Last /= Blue
        or Base_Math.Sqrt (16.0) /= 4.0 then
        raise Program_Error;
    end if;
    X := Narrow'Base'(4.0);
    if X /= 4.0 then
        raise Program_Error;
    end if;
    X := Narrow'Base (5.0);
    I := Smaller'Base (200);
    if X /= 5.0 or I /= 200 then
        raise Program_Error;
    end if;
    F := 2.0 ** N;
    if F /= 8.0 or 2 ** 3 /= 8 or "**" (16.0, 0.5) /= 4.0 then
        raise Program_Error;
    end if;
    L := Ada.Numerics.Long_Elementary_Functions."**" (25.0, 0.5);
    if L /= 5.0 then
        raise Program_Error;
    end if;
    begin
        F := 0.0 ** 0.0;
        raise Program_Error;
    exception
        when Ada.Numerics.Argument_Error => null;
    end;
    begin
        F := (-1.0) ** 0.5;
        raise Program_Error;
    exception
        when Ada.Numerics.Argument_Error => null;
    end;
    begin
        F := 0.0 ** (-1.0);
        raise Program_Error;
    exception
        when Constraint_Error => null;
    end;
    declare
        function "**" (Left, Right : Small'Base) return Small'Base;
        function "**" (Left, Right : Small'Base) return Small'Base is
        begin
            return Left + Right;
        end "**";
    begin
        if Small'Base (2) ** Small'Base (3) /= 5 then
            raise Program_Error;
        end if;
    end;
    declare
        function "**" (Left, Right : Long_Float) return Long_Float;
        pragma Import (C, "**", "__ada_numerics_power");
    begin
        L := 36.0 ** 0.5;
        if L /= 6.0 then
            raise Program_Error;
        end if;
    end;
    Ada.Text_IO.Put_Line ("base and power ok");
end Basepower;

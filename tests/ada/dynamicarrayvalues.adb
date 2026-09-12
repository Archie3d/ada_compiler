with Ada.Text_IO; use Ada.Text_IO;
procedure DynamicArrayValues is
    type Vector is array (Integer range <>) of Integer;
    Calls : Integer := 0;
    function Next return Integer is
    begin
        Calls := Calls + 1;
        return Calls;
    end Next;
    function Make (Size : Integer) return Vector is
        Value : Vector (5 .. Size + 4) := (others => Next);
    begin
        return Value;
    end Make;
    procedure Check (Size : Integer) is
        A, B : Vector := Make (Size);
        type Item is record
            Number : Integer := 17;
        end record;
        type Items is array (Integer range <>) of Item;
        Defaults : Items (1 .. Size);
        procedure Set_First (Value : in out Vector) is
            function Count return Integer is
            begin
                return Value'Length;
            end Count;
        begin
            if Count > 0 then
                Value (Value'First) := 99;
            end if;
        end Set_First;
    begin
        if A'First /= 5 or A'Length /= Size or B'Last /= Size + 4 then
            raise Program_Error;
        end if;
        if Size > 0 then
            if A (5) = B (5) or Defaults (1).Number /= 17 then
                raise Program_Error;
            end if;
            Set_First (A);
            if A (5) /= 99 then
                raise Program_Error;
            end if;
            declare
                Copy : Vector := B (6 .. Size + 4);
            begin
                if Copy'First /= 6 or Copy'Length /= Size - 1 then
                    raise Program_Error;
                end if;
            end;
        end if;
        A := B;
        if A /= B then
            raise Program_Error;
        end if;
    end Check;
begin
    Check (3);
    Check (0);
    if Calls /= 6 then
        raise Program_Error;
    end if;
    Put_Line ("grouped arrays, returns, slices, defaults, and captured parameters");
end DynamicArrayValues;

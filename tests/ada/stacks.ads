generic
    type Element is private;
    Capacity : Integer := 4;
package Stacks is

    procedure Push (Value : Element);
    function Pop return Element;
    function Depth return Integer;
    function Room return Integer;

end Stacks;

package body Stacks is

    type Contents is array (1 .. Capacity) of Element;

    Store : Contents;
    Top   : Integer := 0;

    procedure Push (Value : Element) is
    begin
        if Top = Capacity then
            raise Constraint_Error;
        end if;
        Top := Top + 1;
        Store (Top) := Value;
    end Push;

    function Pop return Element is
        Value : Element := Store (Top);
    begin
        Top := Top - 1;
        return Value;
    end Pop;

    function Depth return Integer is
    begin
        return Top;
    end Depth;

    function Room return Integer is
    begin
        return Capacity;
    end Room;

end Stacks;

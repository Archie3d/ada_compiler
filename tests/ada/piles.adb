package body Piles is

    procedure Push (P : in out Pile; Value : in Integer) is
    begin
        if P.Top = Depth then
            raise Overflow;
        end if;
        P.Top := P.Top + 1;
        P.Items (P.Top) := Value;
    end Push;

    procedure Pop (P : in out Pile; Value : out Integer) is
    begin
        if P.Top = 0 then
            raise Underflow;
        end if;
        Value := P.Items (P.Top);
        P.Top := P.Top - 1;
    end Pop;

    function Size (P : in Pile) return Integer is
    begin
        return P.Top;
    end Size;

    -- Inside the package the representation is in plain view, so the predefined
    -- equality on the record is there to be used.
    function Is_Empty (P : in Pile) return Boolean is
    begin
        return P = Empty;
    end Is_Empty;

end Piles;

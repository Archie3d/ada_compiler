-- What a private type refuses from outside the package that declared it.

with Piles;
use Piles;

procedure Privateerrors is

    P : Pile;
    Q : Pile;

begin
    -- The components are the package's business, so neither naming one nor
    -- writing an aggregate that spells them all out is allowed here.
    P.Top := 3;

    Q := (Items => (others => 0), Top => 0);

    -- Equality comes with the type, but nothing says which of two piles is the
    -- lesser, so the predefined ordering is not offered either.
    if P < Q then
        null;
    end if;

    -- Reaching in through a subtype of it fares no better.
    declare
        subtype Same is Pile;
        R : Same;
    begin
        R.Top := 0;
    end;

end Privateerrors;

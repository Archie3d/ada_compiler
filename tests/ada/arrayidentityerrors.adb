procedure ArrayIdentityErrors is
    type First is array (1 .. 2) of Integer;
    type Second is array (1 .. 2) of Integer;
    type Derived is new First;
    A : First := (1, 2);
    B : Second := (1, 2);
    C : Derived := (1, 2);
    Bad : First := B;
    procedure Take (V : First) is
    begin
        null;
    end Take;
    procedure Defaults (V : First := B) is
    begin
        null;
    end Defaults;
    function Wrong return First is
    begin
        return B;
    end Wrong;
    type Text is array (1 .. 2) of Character;
    T : Text := "ab";
    S : String (1 .. 2) := "ab";
    type Holder is record
        Value : First := B;
    end record;
    H : Holder := (Value => B);
    type Link is access First;
    P : Link := new First'(B);
begin
    A := B;
    A := C;
    Take (B);
    Take (B (1 .. 2));
    A := First'(B);
    if A = B then
        null;
    end if;
    T := S;
    S := T;
    T := T & S;
end ArrayIdentityErrors;

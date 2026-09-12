procedure ComparisonErrors is
    type Reals is array (1 .. 2) of Float;
    A : Reals := (1.0, 2.0);
    B : Reals := (1.0, 3.0);
    type Item is record
        Value : Integer;
    end record;
    type Items is array (1 .. 2) of Item;
    C : Items;
    D : Items;
    Result : Boolean;
begin
    Result := A < B;
    Result := A <= B;
    Result := A > B;
    Result := A >= B;
    Result := C < D;
end ComparisonErrors;

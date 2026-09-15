procedure ArrayLiteralAmbiguity is
    type Text is array (Positive range <>) of Character;
    procedure Take (V : Text) is
    begin
        null;
    end Take;
    procedure Take (V : String) is
    begin
        null;
    end Take;
begin
    Take ("ab");
    Take ("a" & "b");
end ArrayLiteralAmbiguity;

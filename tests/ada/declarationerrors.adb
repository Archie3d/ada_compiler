procedure DeclarationErrors is
    package Hidden is
        type Missing is private;
        Value : constant Integer;
    end Hidden;
    type Unfinished;
begin
    null;
end DeclarationErrors;

procedure ScalarCopyErrors is
    procedure Write (V : out Integer) is
    begin
        V := 1;
    end Write;
    procedure Update (V : in out Integer) is
    begin
        V := V + 1;
    end Update;
    C : constant Integer := 2;
    procedure Read_Only (V : Integer) is
    begin
        Write (V);
        Update (V);
    end Read_Only;
begin
    Write (C);
    Update (C);
    Write (1);
end ScalarCopyErrors;

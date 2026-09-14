procedure RuntimeArrayTypeErrors is
    N : Integer := 3;
    subtype Text is String (1 .. N);
    subtype Again is Text (1 .. N);
    type Matrix is array (1 .. N, 1 .. N) of Integer;
    subtype Again_Matrix is Matrix (1 .. N, 1 .. N);
    subtype Wrong_Rank is Matrix (1 .. N);
    subtype Scalar is Integer range 1 .. N;
    type Holder is record
        Value : Text;
    end record;
    type Rows is array (1 .. 2) of Text;
    Flag : Boolean := True;
    subtype Wrong_Index is String (1 .. Flag);
    type Wrong_Type_Index is array (1 .. Flag) of Integer;
    Bits : Integer := Text'Size;
begin
    Text'Output (1, 1);
end RuntimeArrayTypeErrors;

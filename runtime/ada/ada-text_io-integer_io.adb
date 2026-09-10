package body Ada.Text_IO.Integer_IO is

    use Ada.Text_IO;

    Digit_Set : constant String (1 .. 16) := "0123456789ABCDEF";

    -- Room for the widest thing Put can produce: a sign, a base written as two
    -- digits, the two marks around the number, and the number itself in base
    -- two.
    Text_Length : constant := 40;

    -- Item spelled out in the given base, right justified in Text and without
    -- any padding.  First says where it starts.  Ada writes a number in a base
    -- other than ten as 16#FF#, with the sign in front of the whole thing.
    procedure Image_Of (Item  : in Num;
                        Base  : in Number_Base;
                        Text  : out String;
                        First : out Natural) is
        Left     : Integer := Integer (Item);
        Digit    : Integer;
        Position : Natural := Text_Length + 1;
    begin
        if Base /= 10 then
            Position := Position - 1;
            Text (Position) := '#';
        end if;

        if Left = 0 then
            Position := Position - 1;
            Text (Position) := '0';
        end if;

        -- The remainder is taken from whichever side Left is on and its sign
        -- dropped afterwards, since the most negative value of a type has no
        -- positive counterpart to negate it into.
        while Left /= 0 loop
            Digit := abs (Left rem Base);
            Left := Left / Base;
            Position := Position - 1;
            Text (Position) := Digit_Set (Digit + 1);
        end loop;

        -- The base itself is written the same way round as the number, so its
        -- last digit is laid down first.
        if Base /= 10 then
            Position := Position - 1;
            Text (Position) := '#';
            Position := Position - 1;
            Text (Position) := Digit_Set (Base rem 10 + 1);
            if Base >= 10 then
                Position := Position - 1;
                Text (Position) := Digit_Set (Base / 10 + 1);
            end if;
        end if;

        if Item < 0 then
            Position := Position - 1;
            Text (Position) := '-';
        end if;

        First := Position;
    end Image_Of;

    procedure Put (Item  : in Num;
                   Width : in Field := Default_Width;
                   Base  : in Number_Base := Default_Base) is
        Text  : String (1 .. Text_Length);
        First : Natural;
    begin
        Image_Of (Item, Base, Text, First);
        for I in 1 .. Width - (Text_Length - First + 1) loop
            Ada.Text_IO.Put (' ');
        end loop;
        Ada.Text_IO.Put (Text (First .. Text_Length));
    end Put;

    procedure Put (File  : in File_Type;
                   Item  : in Num;
                   Width : in Field := Default_Width;
                   Base  : in Number_Base := Default_Base) is
        Text  : String (1 .. Text_Length);
        First : Natural;
    begin
        Image_Of (Item, Base, Text, First);
        for I in 1 .. Width - (Text_Length - First + 1) loop
            Ada.Text_IO.Put (File, ' ');
        end loop;
        Ada.Text_IO.Put (File, Text (First .. Text_Length));
    end Put;

    -- A number the instantiated type cannot hold is a Data_Error and not a
    -- Constraint_Error, which is why the range is tested here rather than left
    -- to the assignment below it.
    procedure Store (Word : in String; Item : out Num) is
        Value : Integer;
    begin
        Value := Integer'Value (Word);
        if Value < Integer (Num'First) or else Value > Integer (Num'Last) then
            raise Data_Error;
        end if;
        Item := Num (Value);
    exception
        when Constraint_Error =>
            raise Data_Error;
    end Store;

    procedure Get (Item : out Num; Width : in Field := 0) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (Word, Last);
        Store (Word (1 .. Last), Item);
    end Get;

    procedure Get (File : in File_Type; Item : out Num; Width : in Field := 0) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (File, Word, Last);
        Store (Word (1 .. Last), Item);
    end Get;

end Ada.Text_IO.Integer_IO;

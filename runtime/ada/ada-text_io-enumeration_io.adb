package body Ada.Text_IO.Enumeration_IO is

    use Ada.Text_IO;

    -- 'Image spells a literal in upper case, which is what Upper_Case asks for;
    -- Lower_Case is the same letters the other way.
    procedure Write (Text : in String; Set : in Type_Set) is
    begin
        for I in Text'Range loop
            if Set = Lower_Case and then Text (I) >= 'A' and then Text (I) <= 'Z' then
                Ada.Text_IO.Put (Character'Val (Character'Pos (Text (I)) + 32));
            else
                Ada.Text_IO.Put (Text (I));
            end if;
        end loop;
    end Write;

    procedure Write (File : in File_Type; Text : in String; Set : in Type_Set) is
    begin
        for I in Text'Range loop
            if Set = Lower_Case and then Text (I) >= 'A' and then Text (I) <= 'Z' then
                Ada.Text_IO.Put (File, Character'Val (Character'Pos (Text (I)) + 32));
            else
                Ada.Text_IO.Put (File, Text (I));
            end if;
        end loop;
    end Write;

    procedure Put (Item  : in Enum;
                   Width : in Field := Default_Width;
                   Set   : in Type_Set := Default_Setting) is
        Length : constant Natural := Enum'Image (Item)'Length;
    begin
        Write (Enum'Image (Item), Set);
        -- A field wider than the literal is filled out on the right.
        for I in Length + 1 .. Width loop
            Ada.Text_IO.Put (' ');
        end loop;
    end Put;

    procedure Put (File  : in File_Type;
                   Item  : in Enum;
                   Width : in Field := Default_Width;
                   Set   : in Type_Set := Default_Setting) is
        Length : constant Natural := Enum'Image (Item)'Length;
    begin
        Write (File, Enum'Image (Item), Set);
        for I in Length + 1 .. Width loop
            Ada.Text_IO.Put (File, ' ');
        end loop;
    end Put;

    -- A word that names no literal of the type is a Data_Error here, though
    -- 'Value reports it as the Constraint_Error it is in every other setting.
    procedure Get (Item : out Enum) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (Word, Last);
        Item := Enum'Value (Word (1 .. Last));
    exception
        when Constraint_Error =>
            raise Data_Error;
    end Get;

    procedure Get (File : in File_Type; Item : out Enum) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (File, Word, Last);
        Item := Enum'Value (Word (1 .. Last));
    exception
        when Constraint_Error =>
            raise Data_Error;
    end Get;

end Ada.Text_IO.Enumeration_IO;

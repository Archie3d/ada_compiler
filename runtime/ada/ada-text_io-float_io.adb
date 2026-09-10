package body Ada.Text_IO.Float_IO is

    use Ada.Text_IO;

    -- Laying a real number out is arithmetic on the digits of its
    -- representation, which is the one part of this package the C run time
    -- still does; everything about the type is decided here.
    procedure Write (Item : in Long_Float; Fore : in Field; Aft : in Field; Exp : in Field);
    pragma Import (C, Write, "__ada_put_float");

    procedure Write (File : in File_Type;
                     Item : in Long_Float;
                     Fore : in Field;
                     Aft  : in Field;
                     Exp  : in Field);
    pragma Import (C, Write, "__ada_text_put_float");

    procedure Put (Item : in Num;
                   Fore : in Field := Default_Fore;
                   Aft  : in Field := Default_Aft;
                   Exp  : in Field := Default_Exp) is
    begin
        Write (Long_Float (Item), Fore, Aft, Exp);
    end Put;

    procedure Put (File : in File_Type;
                   Item : in Num;
                   Fore : in Field := Default_Fore;
                   Aft  : in Field := Default_Aft;
                   Exp  : in Field := Default_Exp) is
    begin
        Write (File, Long_Float (Item), Fore, Aft, Exp);
    end Put;

    procedure Get (Item : out Num; Width : in Field := 0) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (Word, Last);
        Item := Num (Ada.Text_IO.Scanning.Real_Value (Word (1 .. Last)));
    end Get;

    procedure Get (File : in File_Type; Item : out Num; Width : in Field := 0) is
        Word : String (1 .. 64);
        Last : Natural;
    begin
        Ada.Text_IO.Scanning.Get_Word (File, Word, Last);
        Item := Num (Ada.Text_IO.Scanning.Real_Value (Word (1 .. Last)));
    end Get;

end Ada.Text_IO.Float_IO;

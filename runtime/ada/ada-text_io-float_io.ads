-- Floating point input and output for one real type.  Aft, the number of
-- figures after the point, comes from the accuracy the type was declared with,
-- so an instance made with a type of ten digits writes ten of them.

with Ada.Text_IO;
with Ada.Text_IO.Scanning;

generic
    type Num is digits <>;

package Ada.Text_IO.Float_IO is

    -- An instance is analysed where it is written rather than where it was
    -- declared, so what it takes from its parent it names.
    use Ada.Text_IO;

    Default_Fore : Field := 2;
    Default_Aft  : Field := Num'Digits - 1;
    Default_Exp  : Field := 3;

    procedure Put (Item : in Num;
                   Fore : in Field := Default_Fore;
                   Aft  : in Field := Default_Aft;
                   Exp  : in Field := Default_Exp);

    procedure Put (File : in File_Type;
                   Item : in Num;
                   Fore : in Field := Default_Fore;
                   Aft  : in Field := Default_Aft;
                   Exp  : in Field := Default_Exp);

    procedure Get (Item : out Num; Width : in Field := 0);

    procedure Get (File : in File_Type; Item : out Num; Width : in Field := 0);

end Ada.Text_IO.Float_IO;

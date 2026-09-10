-- Integer input and output for one integer type.  Everything below is written
-- in Ada: the defaults come from the type the instance was made with, the
-- digits are laid out here, and Get does its own range test.

with Ada.Text_IO;
with Ada.Text_IO.Scanning;

generic
    type Num is range <>;

package Ada.Text_IO.Integer_IO is

    -- An instance is analysed where it is written rather than where it was
    -- declared, so what it takes from its parent it names.
    use Ada.Text_IO;

    Default_Width : Field := Num'Width;
    Default_Base  : Number_Base := 10;

    procedure Put (Item  : in Num;
                   Width : in Field := Default_Width;
                   Base  : in Number_Base := Default_Base);

    procedure Put (File  : in File_Type;
                   Item  : in Num;
                   Width : in Field := Default_Width;
                   Base  : in Number_Base := Default_Base);

    procedure Get (Item : out Num; Width : in Field := 0);

    procedure Get (File : in File_Type; Item : out Num; Width : in Field := 0);

end Ada.Text_IO.Integer_IO;

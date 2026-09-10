-- Reading and writing the literals of an enumeration type.  The names of the
-- literals are what 'Image and 'Value already know, so this package is written
-- entirely in terms of them.

with Ada.Text_IO;
with Ada.Text_IO.Scanning;

generic
    type Enum is (<>);

package Ada.Text_IO.Enumeration_IO is

    -- An instance is analysed where it is written rather than where it was
    -- declared, so what it takes from its parent it names.
    use Ada.Text_IO;

    Default_Width   : Field := 0;
    Default_Setting : Type_Set := Upper_Case;

    procedure Put (Item  : in Enum;
                   Width : in Field := Default_Width;
                   Set   : in Type_Set := Default_Setting);

    procedure Put (File  : in File_Type;
                   Item  : in Enum;
                   Width : in Field := Default_Width;
                   Set   : in Type_Set := Default_Setting);

    procedure Get (Item : out Enum);

    procedure Get (File : in File_Type; Item : out Enum);

end Ada.Text_IO.Enumeration_IO;

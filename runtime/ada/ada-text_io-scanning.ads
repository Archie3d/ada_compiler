-- Reading input in the one way that does not depend on the type being read.
--
-- Ada does not define this package; it belongs to this implementation.  The
-- generic children of Text_IO are written in Ada, and to read a value they
-- have to see where a word ends before they can say what it means.  Deciding
-- where a word ends needs to look one character past it and put that character
-- back, which the C run time can do and Ada.Text_IO cannot.  So the run time
-- hands over the word, and the generic that asked for it decides what it is.

with Ada.Text_IO;

package Ada.Text_IO.Scanning is

    -- The next run of non blank characters, with the blanks before it skipped.
    -- Raises End_Error if the input has nothing left.
    procedure Get_Word (Item : out String; Last : out Natural);
    pragma Import (C, Get_Word, "__ada_get_word");

    procedure Get_Word (File : in File_Type; Item : out String; Last : out Natural);
    pragma Import (C, Get_Word, "__ada_text_get_word");

    -- The real number a word spells out.  Raises Data_Error if it spells none,
    -- which is what Float_IO.Get reports when the input is not a number.
    function Real_Value (Text : in String) return Long_Float;
    pragma Import (C, Real_Value, "__ada_real_value");

end Ada.Text_IO.Scanning;

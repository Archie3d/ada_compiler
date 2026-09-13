-- Access types: an incomplete declaration so that a record and the access type
-- naming it can be written in either order, allocators, dereferences, and
-- giving the storage back.

with Ada.Text_IO;
with Ada.Integer_Text_IO;
with Ada.Unchecked_Deallocation;

use Ada.Text_IO;
use Ada.Integer_Text_IO;

procedure Access_Test is

    type Node;
    type Link is access Node;

    type Node is
        record
            Value : Integer;
            Next  : Link;
        end record;

    type Counter is access Integer;
    type Line is access String (1 .. 5);

    procedure Dispose is new Ada.Unchecked_Deallocation (Node, Link);

    Head  : Link;
    Walk  : Link;
    Ahead : Link;
    Total : Integer := 0;

    Tally : Counter := new Integer'(7);
    Label : Line := new String (1 .. 5);

begin
    -- Built back to front, so that the list comes out in order.
    for I in reverse 1 .. 5 loop
        Head := new Node'(Value => I * I, Next => Head);
    end loop;

    Walk := Head;
    while Walk /= null loop
        Put (Walk.Value, 4);
        Total := Total + Walk.all.Value;
        Walk := Walk.Next;
    end loop;
    New_Line;

    Put ("total"); Put (Total, 5); New_Line;

    -- An access value to something other than a record works the same way.
    Tally.all := Tally.all + 1;
    Put ("tally"); Put (Tally.all, 5); New_Line;

    Label.all := "Ada83";
    Put_Line ("label " & Label.all & " starts with " & Label (1));

    -- Freeing walks the list holding on to the next link first, since the node
    -- it is standing on is gone by the time it would have been read.
    while Head /= null loop
        Ahead := Head.Next;
        Dispose (Head);
        Head := Ahead;
    end loop;

    if Head = null then
        Put_Line ("list freed");
    end if;

    -- null designates nothing, so reaching through it raises Constraint_Error.
    begin
        Put (Head.Value, 3);
        Put_Line ("unreachable");
    exception
        when Constraint_Error =>
            Put_Line ("null dereference");
    end;

end Access_Test;

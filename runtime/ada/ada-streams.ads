-- The unit every stream is measured in.  A stream element is a byte, which is
-- what makes a stream a run of storage that any type can be spelled out into
-- and read back from.

package Ada.Streams is

    type Stream_Element is range 0 .. 255;
    for Stream_Element'Size use 8;

    type Stream_Element_Offset is range -2147483647 .. 2147483647;

    subtype Stream_Element_Count is Stream_Element_Offset range 0 .. 2147483647;

    type Stream_Element_Array is array (Stream_Element_Offset range <>) of Stream_Element;

end Ada.Streams;

-- Gives back the storage an allocator took.  Nothing checks that no other
-- access value still designates the object, which is what "unchecked" says:
-- reaching through one of those afterwards is the program's own doing.

generic
    type Object is private;
    type Name is access Object;

procedure Ada.Unchecked_Deallocation (X : in out Name);

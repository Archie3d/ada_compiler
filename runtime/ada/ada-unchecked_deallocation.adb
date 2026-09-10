procedure Ada.Unchecked_Deallocation (X : in out Name) is

    procedure Release (Address : in Name);
    pragma Import (C, Release, "__ada_deallocate");

begin
    Release (X);
    X := null;
end Ada.Unchecked_Deallocation;

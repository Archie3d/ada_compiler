package RuntimeArrayLibraryErrors is
    N : Integer := 3;
    subtype Text is String (1 .. N);
    type Vector is array (1 .. N) of Integer;
    A : String (1 .. N);
end RuntimeArrayLibraryErrors;

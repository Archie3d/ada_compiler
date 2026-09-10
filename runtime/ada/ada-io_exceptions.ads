-- The exceptions every input output package raises.  The compiler reads this
-- unit before anything else, and the order below matters: the C run time
-- raises these by number, and the numbers follow the order of declaration.

package Ada.IO_Exceptions is

    Status_Error : exception;
    Mode_Error   : exception;
    Name_Error   : exception;
    Use_Error    : exception;
    Device_Error : exception;
    End_Error    : exception;
    Data_Error   : exception;
    Layout_Error : exception;

end Ada.IO_Exceptions;

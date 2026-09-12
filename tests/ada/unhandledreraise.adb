procedure UnhandledReraise is
    Original : exception;
begin
    raise Original;
exception
    when Original =>
        begin
            raise Constraint_Error;
        exception
            when Constraint_Error => null;
        end;
        raise;
end UnhandledReraise;

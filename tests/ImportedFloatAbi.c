float abiFloat(float x)
{
    return x + 1.0f;
}

double abiDouble(double x)
{
    return x + 1.0;
}

float abiMixedFloat(float x, double y, float z)
{
    return x + (float)y * z;
}

double abiMixedDouble(double x, float y, double z)
{
    return x + (double)y * z;
}

void abiUpdate(float x, double y, float* result32, double* result64)
{
    *result32 = x + 2.0f;
    *result64 = y + 3.0;
}

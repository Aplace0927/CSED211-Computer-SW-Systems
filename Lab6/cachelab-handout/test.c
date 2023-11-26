#include <stdio.h>

int main()
{
    int a, b, c, d, M = 61, N = 67;
    for (a = 0; a < N; a += 16)
    {
        for (b = 0; b < M; b += 16)
        {
            for (c = b; (c < b + 16) && (c < M); ++c)
            {
                for (d = a; (d < a + 16) && (d < N); ++d)
                {
                    printf("B[%d][%d] = A[%d][%d]\n", c, d, d, c);
                }
            }
        }
    }
}
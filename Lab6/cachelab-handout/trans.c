/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

#define BLOCK_SIZE 8
#define SUBBLOCK_SIZE 4

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/*
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    int rT, cT, R, C;
    int T0, T1, T2, T3, T4, T5, T6, T7;
    if (M == 32)
    {
        for (R = 0; R < N; R += BLOCK_SIZE)
        {
            for (C = 0; C < M; C += BLOCK_SIZE)
            {
                for (rT = 0; rT < BLOCK_SIZE; rT++)
                {

                    T0 = A[R + rT][C];
                    T1 = A[R + rT][C + 1];
                    T2 = A[R + rT][C + 2];
                    T3 = A[R + rT][C + 3];
                    T4 = A[R + rT][C + 4];
                    T5 = A[R + rT][C + 5];
                    T6 = A[R + rT][C + 6];
                    T7 = A[R + rT][C + 7];

                    B[C][R + rT] = T0;
                    B[C + 1][R + rT] = T1;
                    B[C + 2][R + rT] = T2;
                    B[C + 3][R + rT] = T3;
                    B[C + 4][R + rT] = T4;
                    B[C + 5][R + rT] = T5;
                    B[C + 6][R + rT] = T6;
                    B[C + 7][R + rT] = T7;
                }
            }
        }
    }
    else if (M == 64)
    {
        for (R = 0; R < N; R += BLOCK_SIZE)
        {
            for (C = 0; C < M; C += BLOCK_SIZE)
            {
                /* SubBlockTranspose_SubQuad2 */
                for (rT = R; rT < R + BLOCK_SIZE / 2; rT++)
                {
                    T0 = A[rT][C + 0];
                    T1 = A[rT][C + 1];
                    T2 = A[rT][C + 2];
                    T3 = A[rT][C + 3];
                    T4 = A[rT][C + 4];
                    T5 = A[rT][C + 5];
                    T6 = A[rT][C + 6];
                    T7 = A[rT][C + 7];

                    B[C + 0][rT + 0] = T0;
                    B[C + 1][rT + 0] = T1;
                    B[C + 2][rT + 0] = T2;
                    B[C + 3][rT + 0] = T3;
                    B[C + 0][rT + 4] = T4;
                    B[C + 1][rT + 4] = T5;
                    B[C + 2][rT + 4] = T6;
                    B[C + 3][rT + 4] = T7;
                }

                /* SubBlockTranspose_SubQuad13 */
                for (cT = C; cT < C + BLOCK_SIZE / 2; cT++)
                {
                    T0 = A[R + SUBBLOCK_SIZE + 0][cT];
                    T1 = A[R + SUBBLOCK_SIZE + 1][cT];
                    T2 = A[R + SUBBLOCK_SIZE + 2][cT];
                    T3 = A[R + SUBBLOCK_SIZE + 3][cT];
                    T4 = B[cT][R + 4];
                    T5 = B[cT][R + 5];
                    T6 = B[cT][R + 6];
                    T7 = B[cT][R + 7];

                    B[cT][R + 4] = T0;
                    B[cT][R + 5] = T1;
                    B[cT][R + 6] = T2;
                    B[cT][R + 7] = T3;
                    B[cT + 4][R + 0] = T4;
                    B[cT + 4][R + 1] = T5;
                    B[cT + 4][R + 2] = T6;
                    B[cT + 4][R + 3] = T7;
                }

                /* SubBlockTranspose_SubQuad4 */
                for (rT = R + SUBBLOCK_SIZE; rT < R + BLOCK_SIZE; rT++)
                {
                    T4 = A[rT][C + 4];
                    T5 = A[rT][C + 5];
                    T6 = A[rT][C + 6];
                    T7 = A[rT][C + 7];

                    B[C + 4][rT] = T4;
                    B[C + 5][rT] = T5;
                    B[C + 6][rT] = T6;
                    B[C + 7][rT] = T7;
                }
            }
        }
    }
    else if (M == 61)
    {
        for (R = 0; R < N; R += 16)
        {
            for (C = 0; C < M; C += 16)
            {
                for (rT = C; rT < C + (((M - C) > 16) ? 16 : M - C); rT++)
                {
                    for (cT = R; cT < R + (((N - R) > 16) ? 16 : N - R); cT++)
                    {
                        B[rT][cT] = A[cT][rT];
                    }
                }
            }
        }
    }
}

/*
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);
}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}

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
void transpose_submit(int M, int N, int A[N][M], int B[M][N], int ROW_BLK, int COL_BLK)
{
    int Loop, rT, cT;
    int T0, T1, T2, T3, T4, T5, T6, T7;
    if (M == 32)
    {
        for (Loop = 0; (Loop >> 8) < N; Loop += (0x100 * BLOCK_SIZE))
        {
            for (Loop &= 0xFF00; (Loop & 0xFF) < M; Loop += (0x1 * BLOCK_SIZE))
            {
                for (rT = 0; rT < BLOCK_SIZE; rT++)
                {

                    T0 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF)];
                    T1 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 1];
                    T2 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 2];
                    T3 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 3];
                    T4 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 4];
                    T5 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 5];
                    T6 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 6];
                    T7 = A[((Loop >> 8) & 0xFF) + rT][(Loop & 0xFF) + 7];

                    B[(Loop & 0xFF)][((Loop >> 8) & 0xFF) + rT] = T0;
                    B[(Loop & 0xFF) + 1][((Loop >> 8) & 0xFF) + rT] = T1;
                    B[(Loop & 0xFF) + 2][((Loop >> 8) & 0xFF) + rT] = T2;
                    B[(Loop & 0xFF) + 3][((Loop >> 8) & 0xFF) + rT] = T3;
                    B[(Loop & 0xFF) + 4][((Loop >> 8) & 0xFF) + rT] = T4;
                    B[(Loop & 0xFF) + 5][((Loop >> 8) & 0xFF) + rT] = T5;
                    B[(Loop & 0xFF) + 6][((Loop >> 8) & 0xFF) + rT] = T6;
                    B[(Loop & 0xFF) + 7][((Loop >> 8) & 0xFF) + rT] = T7;
                }
            }
        }
    }
    else if (M == 64)
    {
        for (Loop = 0; ((Loop >> 8) & 0xFF) < N; Loop += (0x100 * BLOCK_SIZE))
        {
            for (Loop &= 0xFF00; (Loop & 0xFF) < M; Loop += (0x1 * BLOCK_SIZE))
            {
                /* SubBlockTranspose_SubQuad2 */
                for (rT = ((Loop >> 8) & 0xFF); rT < ((Loop >> 8) & 0xFF) + BLOCK_SIZE / 2; rT++)
                {
                    T0 = A[rT][(Loop & 0xFF) + 0];
                    T1 = A[rT][(Loop & 0xFF) + 1];
                    T2 = A[rT][(Loop & 0xFF) + 2];
                    T3 = A[rT][(Loop & 0xFF) + 3];
                    T4 = A[rT][(Loop & 0xFF) + 4];
                    T5 = A[rT][(Loop & 0xFF) + 5];
                    T6 = A[rT][(Loop & 0xFF) + 6];
                    T7 = A[rT][(Loop & 0xFF) + 7];

                    B[(Loop & 0xFF) + 0][rT + 0] = T0;
                    B[(Loop & 0xFF) + 1][rT + 0] = T1;
                    B[(Loop & 0xFF) + 2][rT + 0] = T2;
                    B[(Loop & 0xFF) + 3][rT + 0] = T3;
                    B[(Loop & 0xFF) + 0][rT + 4] = T4;
                    B[(Loop & 0xFF) + 1][rT + 4] = T5;
                    B[(Loop & 0xFF) + 2][rT + 4] = T6;
                    B[(Loop & 0xFF) + 3][rT + 4] = T7;
                }

                /* SubBlockTranspose_SubQuad13 */
                for (cT = (Loop & 0xFF); cT < (Loop & 0xFF) + BLOCK_SIZE / 2; cT++)
                {
                    T0 = A[((Loop >> 8) & 0xFF) + SUBBLOCK_SIZE + 0][cT];
                    T1 = A[((Loop >> 8) & 0xFF) + SUBBLOCK_SIZE + 1][cT];
                    T2 = A[((Loop >> 8) & 0xFF) + SUBBLOCK_SIZE + 2][cT];
                    T3 = A[((Loop >> 8) & 0xFF) + SUBBLOCK_SIZE + 3][cT];
                    T4 = B[cT][((Loop >> 8) & 0xFF) + 4];
                    T5 = B[cT][((Loop >> 8) & 0xFF) + 5];
                    T6 = B[cT][((Loop >> 8) & 0xFF) + 6];
                    T7 = B[cT][((Loop >> 8) & 0xFF) + 7];

                    B[cT][((Loop >> 8) & 0xFF) + 4] = T0;
                    B[cT][((Loop >> 8) & 0xFF) + 5] = T1;
                    B[cT][((Loop >> 8) & 0xFF) + 6] = T2;
                    B[cT][((Loop >> 8) & 0xFF) + 7] = T3;
                    B[cT + 4][((Loop >> 8) & 0xFF) + 0] = T4;
                    B[cT + 4][((Loop >> 8) & 0xFF) + 1] = T5;
                    B[cT + 4][((Loop >> 8) & 0xFF) + 2] = T6;
                    B[cT + 4][((Loop >> 8) & 0xFF) + 3] = T7;
                }

                /* SubBlockTranspose_SubQuad4 */
                for (rT = ((Loop >> 8) & 0xFF) + SUBBLOCK_SIZE; rT < ((Loop >> 8) & 0xFF) + BLOCK_SIZE; rT++)
                {
                    T4 = A[rT][(Loop & 0xFF) + 4];
                    T5 = A[rT][(Loop & 0xFF) + 5];
                    T6 = A[rT][(Loop & 0xFF) + 6];
                    T7 = A[rT][(Loop & 0xFF) + 7];

                    B[(Loop & 0xFF) + 4][rT] = T4;
                    B[(Loop & 0xFF) + 5][rT] = T5;
                    B[(Loop & 0xFF) + 6][rT] = T6;
                    B[(Loop & 0xFF) + 7][rT] = T7;
                }
            }
        }
    }
    else if (M == 61)
    {
        for (Loop = 0; (Loop >> 8) < N; Loop += (0x100 * ROW_BLK))
        {
            for (Loop &= 0xFF00; (Loop & 0xFF) < M; Loop += (0x1 * COL_BLK))
            {
                for (rT = (Loop & 0xFF); rT < (Loop & 0xFF) + (((M - (Loop & 0xFF)) > COL_BLK) ? COL_BLK : M - (Loop & 0xFF)); rT++)
                {
                    for (cT = ((Loop >> 8) & 0xFF); cT < ((Loop >> 8) & 0xFF) + (((N - ((Loop >> 8) & 0xFF)) > ROW_BLK) ? ROW_BLK : N - ((Loop >> 8) & 0xFF)); cT++)
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

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

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void transpose_submit_32_32(int M, int N, int A[N][M], int B[M][N]);
void transpose_submit_32_32_r(int M, int N, int A[N][M], int B[M][N]);
void transpose_submit_64_64_r(int M, int N, int A[N][M], int B[M][N]);
void transpose_submit_61_67(int M, int N, int A[N][M], int B[M][N]);
void transpose_submit_61_67_v1(int M, int N, int A[N][M], int B[M][N]);

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
    if (M == 32 && N == 32)
    {
        transpose_submit_32_32_r(M, N, A, B);
    }

    if (M == 64 && N == 64)
    {
        transpose_submit_64_64_r(M, N, A, B);
    }

    if (M == 61 && N == 67) {
        transpose_submit_61_67_v1(M, N, A, B);
    }
}

/*
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started.
 */

char trans_desc_32_32_v1[] = "32 x 32 row-wise scan transpose";
void transpose_submit_32_32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l;
    int BSIZE = 8; // 块的边长

    for (i = 0; i < N; i += BSIZE)
    {
        for (j = 0; j < M; j += BSIZE)
        {
            // 内层两层循环：在当前 [i, i+BSIZE) x [j, j+BSIZE) 的小块内进行转置
            for (k = i; k < i + BSIZE; k++)
            {
                for (l = j; l < j + BSIZE; l++)
                {
                    B[l][k] = A[k][l];
                }
            }
        }
    }
}

char trans_desc_32_32_v2[] = "32 x 32 row-wise scan transpose in register";
void transpose_submit_32_32_r(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l;
    int v0, v1, v2, v3, v4, v5, v6, v7;

    for (i = 0; i < N; i += 8)
    {
        for (j = 0; j < M; j += 8)
        {

            for (k = 0; k < 8; k++)
            {
                v0 = A[i + k][j];
                v1 = A[i + k][j + 1];
                v2 = A[i + k][j + 2];
                v3 = A[i + k][j + 3];
                v4 = A[i + k][j + 4];
                v5 = A[i + k][j + 5];
                v6 = A[i + k][j + 6];
                v7 = A[i + k][j + 7];

                B[j + k][i] = v0;
                B[j + k][i + 1] = v1;
                B[j + k][i + 2] = v2;
                B[j + k][i + 3] = v3;
                B[j + k][i + 4] = v4;
                B[j + k][i + 5] = v5;
                B[j + k][i + 6] = v6;
                B[j + k][i + 7] = v7;
            }

            for (k = 0; k < 8; k++)
            {
                for (l = 0; l < k; l++)
                {
                    v0 = B[j + k][i + l];
                    B[j + k][i + l] = B[j + l][i + k];
                    B[j + l][i + k] = v0;
                }
            }
        }
    }
}

char trans_desc_64_64_v1[] = "64 x 64 row-wise scan transpose in register";
void transpose_submit_64_64_r(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int v0, v1, v2, v3;
    int v4, v5, v6, v7;

    for (i = 0; i < N; i += 8)
    {
        for (j = 0; j < M; j += 8)
        {

            for (k = 0; k < 4; k++)
            {
                v0 = A[i + k][j];
                v1 = A[i + k][j + 1];
                v2 = A[i + k][j + 2];
                v3 = A[i + k][j + 3];

                v4 = A[i + k][j + 4];
                v5 = A[i + k][j + 5];
                v6 = A[i + k][j + 6];
                v7 = A[i + k][j + 7];

                // A[0][0] -> B[0][0], 已转置
                B[j][i + k] = v0;
                B[j + 1][i + k] = v1;
                B[j + 2][i + k] = v2;
                B[j + 3][i + k] = v3;

                // A[0][1] -> B[0][1]
                // 实际上应该是 A[0][1] -> B[1][0], 需要进行进一步处理
                B[j][i + k + 4] = v4;
                B[j + 1][i + k + 4] = v5;
                B[j + 2][i + k + 4] = v6;
                B[j + 3][i + k + 4] = v7;
            }

            // B[0][1] -> B[1][0]
            // A[1][0] -> B[0][1]
            // 行列交叉读, B 腾出空位放置 A 数据
            for (k = 0; k < 4; k++)
            {
                v0 = B[j + k][i + 4];
                v1 = B[j + k][i + 5];
                v2 = B[j + k][i + 6];
                v3 = B[j + k][i + 7];

                v4 = A[i + 4][j + k];
                v5 = A[i + 5][j + k];
                v6 = A[i + 6][j + k];
                v7 = A[i + 7][j + k];

                // A[1][0] -> B[1][0]
                B[j + k][i + 4] = v4;
                B[j + k][i + 5] = v5;
                B[j + k][i + 6] = v6;
                B[j + k][i + 7] = v7;

                // B 数据已经转置好, 原样存放即可
                // B[0][1] -> B[1][0]
                B[j + 4 + k][i] = v0;
                B[j + 4 + k][i + 1] = v1;
                B[j + 4 + k][i + 2] = v2;
                B[j + 4 + k][i + 3] = v3;
            }

            // A[1][1] -> B[1][1]
            for (k = 0; k < 4; k++)
            {
                v0 = A[i + 4 + k][j + 4];
                v1 = A[i + 4 + k][j + 5];
                v2 = A[i + 4 + k][j + 6];
                v3 = A[i + 4 + k][j + 7];

                B[j + 4][i + 4 + k] = v0;
                B[j + 5][i + 4 + k] = v1;
                B[j + 6][i + 4 + k] = v2;
                B[j + 7][i + 4 + k] = v3;
            }
        }
    }
}

char trans_desc_61_67[] = "61 x 67 blocked transpose (16x16 with bounds clamp)";
void transpose_submit_61_67(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, r, c;
    const int BSIZE = 8 * 3; // 16x16 在直接映射缓存下兼顾了空间局部性与失效率
    const int BSIZE1 = 8 * 1;  // 16x16 在直接映射缓存下兼顾了空间局部性与失效率

    for (i = 0; i < N; i += BSIZE)
    {
        for (j = 0; j < M; j += BSIZE1)
        {
            // 自动截断边缘，防止越界，同时无死角覆盖所有元素
            int i_max = (i + BSIZE < N) ? (i + BSIZE) : N;
            int j_max = (j + BSIZE1 < M) ? (j + BSIZE1) : M;

            for (r = i; r < i_max; r++)
            {
                for (c = j; c < j_max; c++)
                {
                    B[c][r] = A[r][c];
                }
            }
        }
    }
}

char trans_desc_61_67_v1[] = "61 x 67 blocked transpose (24x8 register staging)";

void transpose_submit_61_67_v1(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int v0, v1, v2, v3, v4, v5, v6, v7;

    const int TR = 24;
    const int TC = 8;

    for (i = 0; i < N; i += TR)
    {
        for (j = 0; j < M; j += TC)
        {
            int i_max = (i + TR < N) ? (i + TR) : N;
            int j_max = (j + TC < M) ? (j + TC) : M;

            for (k = i; k < i_max; k++)
            {
                // 第一阶段：连续读取 A 的一行
                v0 = A[k][j];
                if (j + 1 < j_max) v1 = A[k][j + 1];
                if (j + 2 < j_max) v2 = A[k][j + 2];
                if (j + 3 < j_max) v3 = A[k][j + 3];
                if (j + 4 < j_max) v4 = A[k][j + 4];
                if (j + 5 < j_max) v5 = A[k][j + 5];
                if (j + 6 < j_max) v6 = A[k][j + 6];
                if (j + 7 < j_max) v7 = A[k][j + 7];

                // 第二阶段：写入 B 的对应位置
                B[j][k] = v0;
                if (j + 1 < j_max) B[j + 1][k] = v1;
                if (j + 2 < j_max) B[j + 2][k] = v2;
                if (j + 3 < j_max) B[j + 3][k] = v3;
                if (j + 4 < j_max) B[j + 4][k] = v4;
                if (j + 5 < j_max) B[j + 5][k] = v5;
                if (j + 6 < j_max) B[j + 6][k] = v6;
                if (j + 7 < j_max) B[j + 7][k] = v7;
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

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);

    // 32 x 32
    // registerTransFunction(transpose_submit_32_32, trans_desc_32_32_v1);

    // 32 x 32 in register
    // registerTransFunction(transpose_submit_32_32_r, trans_desc_32_32_v2);

    // 64 x 64 in register
    // registerTransFunction(transpose_submit_64_64_r, trans_desc_64_64_v1);

    // 61 x 67 in register
    registerTransFunction(transpose_submit_61_67, trans_desc_61_67);
    registerTransFunction(transpose_submit_61_67_v1, trans_desc_61_67_v1);
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

## Part A

### 任务要求

- lab A 的目的是模仿实现 `./csim-ref` 的功能
- 并且要使用 LRU 替换算法.
> 在 Part A 部分暂时不用考虑 I 指令

```bash
❯ ./csim-ref -v -s 4 -E 1 -b 4 -t traces/yi.trace

L 10,1 miss
M 20,1 miss hit
L 22,1 hit
S 18,1 hit
L 110,1 miss eviction
L 210,1 miss eviction
M 12,1 miss eviction hit
hits:4 misses:5 evictions:3
```

结果表示含义:
- hits:缓存命中次数
- misses:缓存未命中次数
- evictions:替换次数


指令含义:
- L(Load): 表示加载指令
- M(Modify): 表示修改指令
- S(Store): 表示存储


### 实现思路

实现步骤:
- 解析参数, 创建相关变量
- 读取 traces 文件内容
   - 解析指令和地址
   - 读取 cache, 统计 cache 命中结果
      - 成功: 直接返回结果, 更新 LRU
      - 失败: 
        - 有空位直接加入 cache, 更新 LRU
        - 无空位, 找出时间戳最小的行覆盖写入, 更新 LRU

#### LRU

在每次更新/读取 cache 的时候直接执行 `timeStamp++` 即可, 然后进行 `evictions` 比较 `timeStamp` 替换掉最小的数据行即可.

#### Modify 指令

- L/S 的实现思路是一样的, 首先读取 cache, 如果没有未命中, 那么再次回写.

- M 实际是执行两次 L/S 的步骤即可.

### 实验结果

```bash
csapp/lab/cachelab-handout                                                                                                                 ❯ make && ./test-csim
gcc -g -Wall -Werror -std=c99 -m64 -o csim csim.c cachelab.c -lm
# Generate a handin tar file each time you compile
tar -cvf liurun-handin.tar  csim.c trans.c
csim.c
trans.c
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27

TEST_CSIM_RESULTS=27
```

### 完整代码

```c
#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <getopt.h>

// 定义缓存配置结构体
typedef struct
{
    int s;
    int E;
    int b;
    char *trace_file;
    int verbose;
} CsimConfig;

// 打印标准的 Usage 帮助信息
void print_usage(char *argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n\n");
    printf("Examples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
}

// 使用 getopt 解析命令行参数并返回配置结构体指针
CsimConfig *parseCliArgv(int argc, char *argv[])
{
    CsimConfig *config = (CsimConfig *)malloc(sizeof(CsimConfig));
    if (!config)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // 初始化默认值
    config->s = 0;
    config->E = 0;
    config->b = 0;
    config->trace_file = NULL;
    config->verbose = 0;

    int opt;
    // 格式串说明：
    // h, v 后面无冒号，表示布尔开关
    // s:, E:, b:, t: 后面有冒号，表示必须携带参数值（自动存入 optarg）
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
            case 'h':
                print_usage(argv);
                free(config);
                exit(0);
            case 'v':
                config->verbose = 1;
                break;
            case 's':
                config->s = atoi(optarg);
                break;
            case 'E':
                config->E = atoi(optarg);
                break;
            case 'b':
                config->b = atoi(optarg);
                break;
            case 't':
                config->trace_file = optarg;
                break;
            default:
                // 遇到非法选项或缺少参数时触发
                print_usage(argv);
                free(config);
                exit(1);
        }
    }

    // 校验必填参数（s, E, b 必须大于 0，trace_file 不能为空）
    if (config->s <= 0 || config->E <= 0 || config->b <= 0 || config->trace_file == NULL)
    {
        fprintf(stderr, "%s: Missing or invalid required command line argument\n", argv[0]);
        print_usage(argv);
        free(config);
        exit(1);
    }

    return config;
}

typedef struct result
{
    unsigned long hits;
    unsigned long misses;
    unsigned long evictions;
} Result;

typedef struct
{
    unsigned long last_used_time;
    unsigned long Tag;
    unsigned long SetIndex;
    unsigned long Block;
} CacheLine;

typedef struct
{
    CsimConfig *argv;
    CacheLine *Set;
    unsigned long timeStamp;
} CacheSet;

void run(CacheSet *set, Result *res)
{
    FILE *file = fopen(set->argv->trace_file, "r");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening trace file: %s\n", set->argv->trace_file);
        return;
    }

    char instruction;
    unsigned long address;
    int size;

    while (fscanf(file, " %c %lx,%d", &instruction, &address, &size) == 3)
    {
        if (instruction == 'I')
        {
            continue; // 忽略所有指令读取访问
        }

        // 1. 提取 Block Offset
        unsigned long offset_mask = (1ULL << set->argv->b) - 1;
        unsigned long offset = address & offset_mask;

        // 2. 提取 Set Index
        unsigned long set_mask = (1ULL << set->argv->s) - 1;
        unsigned long set_index = (address >> set->argv->b) & set_mask;

        // 3. 提取 Tag
        unsigned long tag = address >> (set->argv->s + set->argv->b);

        unsigned long base_offset = set_index * set->argv->E;

        // M 指令代表一次 Load 和一次 Store，需连续访问 2 次
        int epoch = (instruction == 'M') ? 2 : 1;

        for (int j = 1; j <= epoch; j++)
        {
            set->timeStamp++;

            // 查询 cache
            int query_index;
            for (query_index = 0; query_index < set->argv->E; query_index++)
            {
                int idx = base_offset + query_index;
                if ((set->Set[idx].last_used_time != 0) && (set->Set[idx].Tag == tag))
                {
                    break;
                }
            }

            // 命中 (Hit)
            if (query_index != set->argv->E)
            {
                res->hits++;
                set->Set[base_offset + query_index].last_used_time = set->timeStamp;
                continue;
            }

            // 未命中 (Miss)
            res->misses++;

            // 从内存读入数据到 cache
            int evict_index;
            int need_evict_index = -1;
            int is_empty_slot = 0;
            unsigned long minTimeStamp = ULONG_MAX;

            CacheLine newLine = {set->timeStamp, tag, set_index, offset};

            for (evict_index = 0; evict_index < set->argv->E; evict_index++)
            {
                int idx = base_offset + evict_index;

                // 找到空位直接插入
                if (set->Set[idx].last_used_time == 0)
                {
                    need_evict_index = evict_index;
                    is_empty_slot = 1;
                    break;
                }

                // 记录 LRU（时间戳最小）的一行
                if (set->Set[idx].last_used_time < minTimeStamp)
                {
                    need_evict_index = evict_index;
                    minTimeStamp = set->Set[idx].last_used_time;
                }
            }

            if (!is_empty_slot)
            {
                res->evictions++; // 组满替换，累加驱逐计数
            }

            set->Set[base_offset + need_evict_index] = newLine;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[])
{
    // 1. 调用 getopt 解析命令行参数
    CsimConfig *config = parseCliArgv(argc, argv);

    // 2. 初始化统计与缓存
    Result res = {0, 0, 0};
    CacheSet *set = (CacheSet *)malloc(sizeof(CacheSet));
    if (set == NULL)
    {
        fprintf(stderr, "set malloc fault\n");
        free(config);
        return 1;
    }

    set->argv = config;
    set->timeStamp = 0;

    int num_sets = 1 << config->s;
    int total_lines = num_sets * config->E;

    set->Set = (CacheLine *)calloc(total_lines, sizeof(CacheLine));
    if (set->Set == NULL)
    {
        fprintf(stderr, "cache lines calloc fault\n");
        free(set);
        free(config);
        return 1;
    }

    // 3. 执行仿真
    run(set, &res);

    // 4. 打印 CS:APP 官方测试摘要并释放资源
    printSummary(res.hits, res.misses, res.evictions);

    free(set->Set);
    free(set);
    free(config);

    return 0;
}
```

## Part B

> 32 * 32 和 64 * 64 的具体的思路参考的这个[更适合北大宝宝体质的 Cache Lab 踩坑记](https://arthals.ink/blog/cache-lab#32x32)

cache 配置为 $(s=5, E=1, b=5)$: 
$$ c = 1 << s * E * 1 << b = 32 * 1 * 32 = 1024 Byte $$

那么每次可以存放的 int 数据块为: 
$$1024 / 8 = 256$$

每一行可以保存的 int 数据为 
$$ 32 / 4 = 8 $$


32 * 32 和 64 * 64 存在的最主要问题就是：

   - 块大导致缓存被替换：大块会迫使还未使用的块被替换，导致出现 miss 次数增加。
   - A B 之间存在 cache 冲突（Conflict Miss）：由于在 32 * 32 和 64 * 64 中， A 与 B 之间存在这 1024 的倍数差值，导致*每达到一定倍数就会将还未使用的 cache 块替换，导致 miss 出现。


**Conflict Miss 分析**

| Raw Trace       | Op | Address      | Tag (Hex) | Set | Offset | Set_Bits Offset_Bits | Status        | Evicted Block Base |
|-----------------|----|--------------|-----------|-----|--------|---------------------|---------------|--------------------|
| L 0010e1c0,4    | L  | 0x0010e1c0   | 0x438     | 14  | 0      | 01110 00000         | miss eviction | 0x0018e1c0         |
| S 0014e1c0,4    | S  | 0x0014e1c0   | 0x538     | 14  | 0      | 01110 00000         | miss eviction | 0x0010e1c0         |
| L 0010e1c4,4    | L  | 0x0010e1c4   | 0x438     | 14  | 4      | 01110 00100         | miss eviction | 0x0014e1c0         |
| S 0014e240,4    | S  | 0x0014e240   | 0x538     | 18  | 0      | 10010 00000         | miss          | -                  |
| L 0010e1c8,4    | L  | 0x0010e1c8   | 0x438     | 14  | 8      | 01110 01000         | hit           | -                  |
| S 0014e2c0,4    | S  | 0x0014e2c0   | 0x538     | 22  | 0      | 10110 00000         | miss          | -                  |
| L 0010e1cc,4    | L  | 0x0010e1cc   | 0x438     | 14  | 12     | 01110 01100         | hit           | -                  |
| S 0014e340,4    | S  | 0x0014e340   | 0x538     | 26  | 0      | 11010 00000         | miss          | -                  |

> 这是我拿 AI 搓的分析 trace 文件的脚本，详细代码在 [trace 文件分析代码](#other)

我们分析这一串 trace 文件的一部分：
- **L 0010e1c0,4**：Load A[0][0]，cache 中没有对应值，导致需要进行冷启动到 set = 0 中（冷启动）
- **S 0014e1c0,4**：将 A[0][0] 储存到 B[0][0]， 但是 （0x0014e1c0 - 0x0010e1c0）% 1024 = 0，因此，对应的 Set 都是 0，那么就会直接将 A cache set=0 替换（Conflict Miss）
- **L 0010e1c4,4**：读取 A[0][1]，set=0 再次替换，导致又一次出现 miss


相反，61 * 67 天然存在一定的偏差，使得数据在 cache 中存在一定偏差，使得对应的 set 呈现散列分布的状态，因此可以进行简单的分块处理即可。

### 32 x 32

我们来简单分析一下 cache 的整个工作状态: cache 初始为全新的,每次加载新数据都要冷启动, 如果直接使用 simple transporse 所出现的 misses 次数:

misses: 128 (A 冷启动) + 32 (对角线冲突缺失) + 32 * 32 (B 冷启动 + 更新列导致的 evictions) = 1184

evictions: 1184 - 32 (cache 为空) = 1152

按照之前的分析，我们优化方向就分为两部分：

1. 对块进行细分
2. 解决 Conflict Miss 问题

#### 分块处理

```c
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
```

#### 优化 Conflict Miss

我们可以一次性的读取 cache 的一行，也就是一次性读取 8 个 int 数据，类似循环展开的方法，这样我们就可以直接放在寄存器中，然后再放到 B 中，那么我们就避免了单次读触发的 miss。

但是，这样还是不能完全优化，每次读取的时候对角线上的值还是会触发 Conflict Miss。

下面举例说明，为什么 B[1][1] 会产生 Miss，此时（i=0，j=0）：

- 读取 A[0][1]，随后写入 B[1][0]，此时 set=18，还有七个空槽
    ```
    S 0014e240,4
    Set = 18
    Status = miss eviction
    ```
- 执行 k=1，读取 A[1][0]，此时正好也是 set=18，导致 B 的 cache line 被替换
    ```
    L 0010e240,4
    Set = 18
    Status = miss eviction
    Evicted = 0x0014e240
    ```

- 再次写入 B[1][1]，发现未命中，随后出现一次 miss
    ```
    S 0014e244,4
    Set = 18
    Status = miss eviction
    Evicted = 0x0010e240
    ```

为了避免出现这个情况，因此我们实现的思路是，设矩阵 \(A\) 按照行和列划分为如下子矩阵：

$$
A=
\begin{bmatrix}
A_{11} & A_{12} & \cdots & A_{1q}\\
A_{21} & A_{22} & \cdots & A_{2q}\\
\vdots & \vdots & \ddots & \vdots\\
A_{p1} & A_{p2} & \cdots & A_{pq}
\end{bmatrix}
$$

那么我们将其复制到 B 矩阵，随后再根据分块矩阵的公式对子块进行转置，即

$$
B^T=
\begin{bmatrix}
A_{11}^T & A_{21}^T & \cdots & A_{p1}^T\\
A_{12}^T & A_{22}^T & \cdots & A_{p2}^T\\
\vdots & \vdots & \ddots & \vdots\\
A_{1q}^T & A_{2q}^T & \cdots & A_{pq}^T
\end{bmatrix}
$$

比如一次性读取 A[0][0:7] 的值到寄存器中，随后再原样放置到 B[0][0:7] 中，这样不会导致 conflict 发生。

> A[0][0:7] 表示读取 A 第 0 行，0 到 7 列的数据

随后我们对这个分块进行转置，cache 中都是 B 子块的数据，不会出现被替换的情况

优化之后的代码为

```c
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
                // 读取 A[0][0:7]
                v0 = A[i + k][j];
                v1 = A[i + k][j + 1];
                v2 = A[i + k][j + 2];
                v3 = A[i + k][j + 3];
                v4 = A[i + k][j + 4];
                v5 = A[i + k][j + 5];
                v6 = A[i + k][j + 6];
                v7 = A[i + k][j + 7];

                // 原样放置到 B[0][0:7]
                B[j + k][i] = v0;
                B[j + k][i + 1] = v1;
                B[j + k][i + 2] = v2;
                B[j + k][i + 3] = v3;
                B[j + k][i + 4] = v4;
                B[j + k][i + 5] = v5;
                B[j + k][i + 6] = v6;
                B[j + k][i + 7] = v7;
            }

            // 转置子块
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
```

#### 实验结果

```bash
make && ./test-trans -M 32 -N 32 

# 最简单的转置
hits:869, misses:1184, evictions:1152  

# 分块
hits:1709, misses:344, evictions:312

# 分块基础上处理 Conflict Miss，其中 4 次为其他的情况导致的 miss
hits:3585, misses:260, evictions:228
```

### 64 x 64

根据上述计算，我们可以得到 A 每 4 行就会直接填满 cache，因此我们的分块就不能和 32 x 32 一样直接选取 8 做为子块大小。但是，我们可以宏观上按照 8 x 8 进行搬去数据块，然后再从 8 x 8 中进行微观分块。

具体思路就是使用右上角的子块进行暂存，具体思路：

1. 初始化状态 A 为

$$
A\in\mathbb{R}^{8\times 8},\qquad
A_{i,j}\in\mathbb{R}^{4\times 4}
$$

$$
A=
\begin{bmatrix}
A_{0,0} & A_{0,1}\\
A_{1,0} & A_{1,1}\\
\end{bmatrix}
$$

$$
B=
\begin{bmatrix}
* & * \\
* & * \\
\end{bmatrix}
$$

2. 首先读取 $A^T_{0,0:1}$ 数据，然后分别放置到 $B[0][0], B[0][1]$ 中

$$
A=
\begin{bmatrix}
A_{0,0} & A_{0,1}\\
A_{1,0} & A_{1,1}\\
\end{bmatrix}
$$

$$
B=
\begin{bmatrix}
A^T_{0,0} & A^T_{0,1} \\
* & * \\
\end{bmatrix}
$$

3. 将 $B_{0, 1}$ 的值放置到 $B_{1, 0}$ 中，并且同时读取 $A^T_{0, 1}$ 到 $B_{1, 0}$
 
$$
A=
\begin{bmatrix}
A_{0,0} & A_{0,1}\\
A_{1,0} & A_{1,1}\\
\end{bmatrix}
$$

$$
B=
\begin{bmatrix}
A^T_{0,0} &  A^T_{1,0} \\
A^T_{0,1} & * \\
\end{bmatrix}
$$

4. 最后将 $A^T_{1, 1}$ 放置到 $B_{1, 1}$ 中

$$
A=
\begin{bmatrix}
A_{0,0} & A_{0,1}\\
A_{1,0} & A_{1,1}\\
\end{bmatrix}
$$

$$
B=
\begin{bmatrix}
A^T_{0,0} &  A^T_{1,0} \\
A^T_{0,1} & A^T_{1,1} \\
\end{bmatrix}
$$

### 61 * 67

首先我尝试的是进行 8 x 8 分块，但是发现效果不是特别理想，随后尝试了不同的分块组合，结果如下：

| 分块大小 | Miss 值 |
|:---:|---:|
| 8 × 8 | 2200+ ? |
| 8 × 16 | 2000+ ?|
| 8 × 24 | 2275 |
| 16 × 8 | 1954 |
| 24 × 8 | **1898** |
| 32 × 8 | 1963 |

> ? 表示没有记录清晰的数据，只记得一个大概的数字，太懒了 qwq

欸，可以发现 24 x 8 已经可以拿到满分了，原因已经 part B 开头解释过了。这是只用分块的代码

```c
void transpose_submit_61_67(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, r, c;
    const int BSIZE = 8 * 3; 
    const int BSIZE1 = 8 * 1;  

    for (i = 0; i < N; i += BSIZE)
    {
        for (j = 0; j < M; j += BSIZE1)
        {
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
```

我们再按照之前存放寄存器的思路进行进一步优化：

```c
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
```

得到结果, 1898 -> 1744，已经小于 2000，可以拿满分了 :)
``` bash
hits:6435, misses:1744, evictions:1712
```

### handin

`driver.py` 使用的语法为 python 2.7 的语法类型，还需要配置一下对应 python 版本，我直接使用 conda 进行处理。

```bash
conda create -n cacheLab python=2.7 -y
conda activate cacheLab
python driver.py
```

然后我们就可以得到最终的评分啦 :)

``` bash
Part A: Testing cache simulator
Running ./test-csim
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27


Part B: Testing transpose function
Running ./test-trans -M 32 -N 32
Running ./test-trans -M 64 -N 64
Running ./test-trans -M 61 -N 67

Cache Lab summary:
                        Points   Max pts      Misses
Csim correctness          27.0        27
Trans perf 32x32           8.0         8         260
Trans perf 64x64           8.0         8        1228
Trans perf 61x67          10.0        10        1744
          Total points    53.0        53
```

### other

还有拿 AI 写的对 trace 文件分析的脚本，不过我也没咋用 > w <，用法为

```bash
python parse_trace.py trace.f3 trace.f3_p
```



```python
#!/usr/bin/env python3
import sys
import re
from typing import Optional, Dict, Any, Tuple

DEFAULT_S = 5
DEFAULT_E = 1
DEFAULT_B = 5

def print_help():
    help_text = """
Usage:
  python3 parse_trace.py <input_trace> <output_file> [s] [E] [b]
  python3 parse_trace.py -h | --help

Description:
  解析 CS:APP Cache Lab 格式的内存轨迹文件（trace），输出结构化的
  Cache 映射详情（Tag, Set Index, Block Offset），并模拟缓存访问
  状态（hit, miss, miss eviction）。
  当发生 eviction 时，会打印出被替换块的基地址（起始地址）。
"""
    print(help_text.strip())

class CacheSimulator:
    """带被替换地址追溯的 Cache 模拟器"""
    def __init__(self, s: int, E: int, b: int):
        self.s = s
        self.E = E
        self.b = b
        self.num_sets = 1 << s
        self.cache = [[{"valid": False, "tag": 0, "lru": 0} for _ in range(E)] for _ in range(self.num_sets)]
        self.time_counter = 0

    def access(self, tag: int, set_idx: int) -> Tuple[str, Optional[int]]:
        """
        返回: (status_str, evicted_block_addr)
        若没有发生替换，evicted_block_addr 为 None
        """
        self.time_counter += 1
        lines = self.cache[set_idx]

        # 1. 查找 Hit
        for line in lines:
            if line["valid"] and line["tag"] == tag:
                line["lru"] = self.time_counter
                return "hit", None

        # 2. 查找是否有空位 (Cold Miss)
        for line in lines:
            if not line["valid"]:
                line["valid"] = True
                line["tag"] = tag
                line["lru"] = self.time_counter
                return "miss", None

        # 3. 组满替换 (Miss + Eviction)
        lru_idx = min(range(self.E), key=lambda idx: lines[idx]["lru"])
        
        # 计算被逐出块的起始内存基地址
        old_tag = lines[lru_idx]["tag"]
        evicted_addr = (old_tag << (self.s + self.b)) | (set_idx << self.b)

        # 更新为新行
        lines[lru_idx]["tag"] = tag
        lines[lru_idx]["lru"] = self.time_counter
        return "miss eviction", evicted_addr


def parse_trace_line(line: str, s: int, b: int) -> Optional[Dict[str, Any]]:
    match = re.search(r"([LSM])\s+([0-9a-fA-F]+),(\d+)", line)
    if not match:
        return None

    op = match.group(1)
    addr = int(match.group(2), 16)
    size = int(match.group(3))

    block_offset = addr & ((1 << b) - 1)
    set_index = (addr >> b) & ((1 << s) - 1)
    tag = addr >> (s + b)

    total_low_bits = s + b
    low_binary = f"{(addr & ((1 << total_low_bits) - 1)):0{total_low_bits}b}"
    set_bits = low_binary[:s]
    offset_bits = low_binary[s:]

    return {
        "op": op,
        "addr": addr,
        "size": size,
        "tag": tag,
        "set_index": set_index,
        "block_offset": block_offset,
        "set_bits": set_bits,
        "offset_bits": offset_bits,
    }


def process_file(input_filepath: str, output_filepath: str, s: int = DEFAULT_S, E: int = DEFAULT_E, b: int = DEFAULT_B):
    sim = CacheSimulator(s, E, b)
    
    # 增加一列: Evicted Addr
    header = (
        f"{'Raw Trace':<18} | {'Op':<2} | {'Address':<10} | {'Tag (Hex)':<10} | "
        f"{'Set':<5} | {'Offset':<6} | {'Set_Bits Offset_Bits':<20} | {'Status':<15} | {'Evicted Block Base'}\n"
    )
    separator = "-" * 122 + "\n"

    total_hits = 0
    total_misses = 0
    total_evictions = 0

    with open(input_filepath, "r", encoding="utf-8") as fin, open(output_filepath, "w", encoding="utf-8") as fout:
        fout.write(f"# Cache Configuration: s={s} (Sets={1<<s}), E={E}, b={b} (BlockSize={1<<b}B)\n")
        fout.write(header)
        fout.write(separator)

        for line in fin:
            line_str = line.strip()
            if not line_str or line_str.startswith("I"):
                continue

            parsed = parse_trace_line(line_str, s, b)
            if not parsed:
                continue

            op = parsed["op"]
            tag = parsed["tag"]
            set_idx = parsed["set_index"]

            access_times = 2 if op == "M" else 1
            statuses = []
            evicted_addrs = []

            for _ in range(access_times):
                status, evicted_addr = sim.access(tag, set_idx)
                statuses.append(status)
                if evicted_addr is not None:
                    evicted_addrs.append(f"0x{evicted_addr:08x}")
                if "hit" in status:
                    total_hits += 1
                if "miss" in status:
                    total_misses += 1
                if "eviction" in status:
                    total_evictions += 1

            status_str = " ".join(statuses)
            evicted_str = ", ".join(evicted_addrs) if evicted_addrs else "-"
            binary_split = f"{parsed['set_bits']} {parsed['offset_bits']}"

            fout.write(
                f"{line_str:<18} | {op:<2} | 0x{parsed['addr']:08x} | 0x{tag:<8x} | "
                f"{set_idx:<5} | {parsed['block_offset']:<6} | {binary_split:<20} | "
                f"{status_str:<15} | {evicted_str}\n"
            )

        fout.write(separator)
        fout.write(f"# Summary: hits:{total_hits} misses:{total_misses} evictions:{total_evictions}\n")

    print(f"[+] 解析完成！结果已写入: {output_filepath}")
    print(f"    统计结果: hits={total_hits}, misses={total_misses}, evictions={total_evictions}")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] in ("-h", "--help"):
        print_help()
        sys.exit(0)
        
    if len(sys.argv) < 3:
        print("Usage: python3 parse_trace.py <input_trace_file> <output_result_file> [s] [E] [b]")
        sys.exit(1)

    in_file = sys.argv[1]
    out_file = sys.argv[2]
    s_val = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_S
    E_val = int(sys.argv[4]) if len(sys.argv) > 4 else DEFAULT_E
    b_val = int(sys.argv[5]) if len(sys.argv) > 5 else DEFAULT_B

    process_file(in_file, out_file, s_val, E_val, b_val)
```
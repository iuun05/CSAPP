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
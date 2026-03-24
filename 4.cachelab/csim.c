#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

typedef unsigned long long ull;

// cache行
typedef struct{
    int valid; // 有效位
    ull tag;   // 标记位
    int time;  // 时间戳：记录上次被访问的时间，值越小说明越久没被碰过
}cache_line;

// cache组
typedef struct{
    cache_line *lines;
}cache_set;

// cache
typedef struct{
    cache_set *sets;
    int s, E, b, S;
}Cache;

// 命中、未命中、驱逐的次数
typedef struct{
    int hits, misses, evictions;
}cache_stats;

// 初始化cache
Cache init_cache(int s, int E, int b){
    Cache cache;
    cache.s = s;
    cache.E = E;
    cache.b = b;
    cache.S = 1 << s;

    cache.sets = (cache_set *)malloc(cache.S * sizeof(cache_set)); // cache全部组的大小
    for (int i = 0; i < cache.S; i++){
        cache.sets[i].lines = (cache_line * )malloc(E * sizeof(cache_line)); // 每组全部行的大小
        for (int j = 0; j < E; j++){
            cache.sets[i].lines[j].valid = 0;
            cache.sets[i].lines[j].tag = 0;
            cache.sets[i].lines[j].time = 0;
        }
    }
    return cache;
}

// 释放cache
void free_cache(Cache *cache){
    for (int i = 0; i < cache -> S; i++){
        free(cache -> sets[i].lines); // 释放每个组的全部的行
    }
    free(cache -> sets); // 释放全部的组
}

// 访存
void access_cache(Cache *cache, cache_stats *stats, ull addr, int *global_time){
    ull tag = addr >> (cache -> s + cache -> b); // 获取tag
    int idx = (addr >> cache -> b) & ((1 << cache -> s) - 1); // 获取组号

    cache_set *set = &cache -> sets[idx];
    int empty_id = -1;
    int evict_id = -1;
    int min_time = 2e9;

    (*global_time)++;

    // 遍历这个组，判断是否命中
    for (int i = 0; i < cache -> E; i++){
        if (set -> lines[i].valid && set -> lines[i].tag == tag){
            stats -> hits++;
            set -> lines[i].time = *global_time; // 刷新存活时间
            return;
        }
    }

    stats -> misses++;

    // 若未命中，寻找是否空位，并记录最久未使用的行
    for (int i = 0; i < cache -> E; i++){
        if (!set -> lines[i].valid){
            empty_id = i;
            break;
        }
        if (set -> lines[i].time < min_time){
            min_time = set -> lines[i].time;
            evict_id = i;
        }
    }

    if (empty_id != -1){
        set -> lines[empty_id].valid = 1;
        set -> lines[empty_id].tag = tag;
        set -> lines[empty_id].time = *global_time;
    }
    else{
        stats -> evictions++;
        set -> lines[evict_id].tag = tag;
        set -> lines[evict_id].time = *global_time;
    }
}

void print_help()
{
    printf("** A Cache Simulator by Deconx\n");
    printf("Usage: ./csim-ref [-hv] -s <num> -E <num> -b <num> -t <file>\n");
    printf("Options:\n");
    printf("-h         Print this help message.\n");
    printf("-v         Optional verbose flag.\n");
    printf("-s <num>   Number of set index bits.\n");
    printf("-E <num>   Number of lines per set.\n");
    printf("-b <num>   Number of block offset bits.\n");
    printf("-t <file>  Trace file.\n\n\n");
    printf("Examples:\n");
    printf("linux>  ./csim -s 4 -E 1 -b 4 -t traces/yi.trace\n");
    printf("linux>  ./csim -v -s 8 -E 2 -b 4 -t traces/yi.trace\n");
}

Cache init_cache(int s, int E, int b);
void free_cache(Cache *cache);
void access_cache(Cache *cache, cache_stats *stats, ull addr, int *global_time);

int main(int argc, char *argv[])
{
    int opt;
    int s = 0, E = 0, b = 0;
    char trace_file[1000];

    while (-1 != (opt = getopt(argc, argv, "hvs:E:b:t:")))
    {
        switch (opt)
        {
        case 'h':
            print_help();
            exit(0);
        case 'v':
            //verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            strcpy(trace_file, optarg);
            break;
        default:
            print_help();
            exit(-1);
        }
    }
    
    Cache cache = init_cache(s, E, b);
    cache_stats stats = {0, 0, 0};
    int global_time = 0;

    FILE *fp = fopen(trace_file, "r");
    if (!fp){
        exit(-1);
    }

    char op[5];
    ull addr;
    int size;

    while (fscanf(fp, "%s %llx, %d", op, &addr, &size) != EOF){
        if (op[0] == 'I') continue;

        access_cache(&cache, &stats, addr, &global_time);

        if (op[0] == 'M'){
            access_cache(&cache, &stats, addr, &global_time);
        }
    }

    fclose(fp);
    free_cache(&cache);

    printSummary(stats.hits, stats.misses, stats.evictions);

    return 0;
}

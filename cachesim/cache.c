#include "common.h"
#include <inttypes.h>
#include <time.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;
static uint64_t hit_num = 0;
static uint64_t miss_num = 0;
static uint64_t hit_read_num = 0;
static uint64_t miss_read_num = 0;
static uint64_t hit_write_num = 0;
static uint64_t miss_write_num = 0;
static uint64_t total_cnt = 0;

struct timespec time_now[2] ={{0, 0}, {0, 0}};
int64_t read_time = 0, write_time = 0;
int64_t cache_read_time  = 0,  cache_write_time = 0;

static int CACHE_GROUP_NUM;
static int CACHE_LINE_NUM;
static int INDEX_WIDTH;

void cycle_increase(int n) { cycle_cnt += n; }
void total_cnt_increase(int n){ total_cnt += 1;} 

typedef struct
{
  uint8_t data[BLOCK_SIZE];
  uint32_t tag;
  bool valid;
  bool dirty;

}cache_block;

cache_block* cache;

// TODO: implement the following functions
// 从 cache 中读出 addr 地址处的 4 字节数据
// 若缺失，需要先从内存中读入数据
uint32_t cache_read(uintptr_t addr, bool debug) {

  uintptr_t addr_valid = addr & ~0x3;

  uint32_t block_addr = addr_valid & (BLOCK_SIZE - 1);
  uint32_t group_id = ( (addr_valid >> BLOCK_WIDTH) & (CACHE_GROUP_NUM-1));
  uint32_t tag = (addr_valid >>  (INDEX_WIDTH + BLOCK_WIDTH)); 
  uint32_t block_num = addr >> BLOCK_WIDTH;

  uint32_t *ret;

  total_cnt_increase(1);
  
  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid && cache[group_id * CACHE_LINE_NUM + i].tag == tag){
        if(debug){
          printf("read is hit \n");
          printf("err tag the tag is %x \n", tag);
          printf("dirty ? %d \n", cache[group_id * CACHE_LINE_NUM + i].dirty);
        }

        ret = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        hit_num += 1;
        hit_read_num += 1;
        return *ret;
    }
  }

  for(int i = 0; i < CACHE_LINE_NUM; i++){
    if(cache[group_id * CACHE_LINE_NUM + i].valid == false){
        if(debug){
          printf("read is unhit \n");
        }
        miss_num += 1;
        miss_read_num += 1;

        mem_read(block_num, cache[group_id * CACHE_LINE_NUM + i].data);
        cache[group_id * CACHE_LINE_NUM + i].valid = true;
        cache[group_id * CACHE_LINE_NUM + i].dirty = false;
        cache[group_id * CACHE_LINE_NUM + i].tag = tag;
        ret = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        return *ret;
    }
  }



  miss_num ++;
  miss_read_num += 1;

  int line_rand = rand() % CACHE_LINE_NUM;
  #ifdef WRITE_BACK
    if(cache[group_id * CACHE_LINE_NUM + line_rand].dirty == true){
      uintptr_t addr_find = (cache[group_id * CACHE_LINE_NUM + line_rand].tag << (INDEX_WIDTH)) | group_id ;
      mem_write(addr_find, cache[group_id * CACHE_LINE_NUM + line_rand].data); 
      cache[group_id * CACHE_LINE_NUM + line_rand].dirty = false;
    }
  #endif
  mem_read(block_num, cache[group_id * CACHE_LINE_NUM + line_rand].data);
  cache[group_id * CACHE_LINE_NUM + line_rand].valid = true;
  cache[group_id * CACHE_LINE_NUM + line_rand].tag = tag; 
  
  ret = (void *)(cache[group_id * CACHE_LINE_NUM + line_rand].data) + (block_addr);

  return *ret;
}

// 往 cache 中 addr 地址所属的块写入数据 data，写掩码为 wmask
// 例如当 wmask 为 0xff 时，只写入低8比特
// 若缺失，需要从先内存中读入数据
void cache_write(uintptr_t addr, uint32_t data, uint32_t wmask) {

  uintptr_t addr_valid = addr & ~0x3;

  uint32_t block_addr = addr_valid & (BLOCK_SIZE - 1);
  uint32_t group_id = ( (addr_valid >> BLOCK_WIDTH) & (CACHE_GROUP_NUM-1));
  uint32_t tag = (addr_valid >> (INDEX_WIDTH + BLOCK_WIDTH)); 

  uint32_t block_num = addr >> BLOCK_WIDTH;

  total_cnt_increase(1);


  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid && cache[group_id * CACHE_LINE_NUM + i].tag == tag){

        uint32_t *temp  = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        *temp = (*temp & ~wmask) | (data & wmask);

        #ifdef WRITE_BACK 
          cache[group_id * CACHE_LINE_NUM + i].dirty = true;          
        #else
          mem_write(block_num, cache[group_id * CACHE_LINE_NUM + i].data);
        #endif

        hit_num += 1;
        hit_write_num += 1;
        
        return ;
    }
  }

  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid == false){

        miss_num += 1;
        miss_write_num += 1;
        mem_read(block_num, cache[group_id * CACHE_LINE_NUM + i].data);

        uint32_t *temp  = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        
        cache[group_id * CACHE_LINE_NUM + i].tag = tag;
        cache[group_id * CACHE_LINE_NUM + i].valid = true;

        *temp = (*temp & ~wmask) | (data & wmask);
        #ifdef WRITE_BACK
          cache[group_id * CACHE_LINE_NUM + i].dirty  = true;
        #else
          mem_write(block_num, cache[group_id * CACHE_LINE_NUM + i].data);
        #endif

        return ;
    }
  }

  miss_num += 1;
  miss_write_num += 1;

  int line_rand = rand() % CACHE_LINE_NUM;
  #ifdef WRITE_BACK
    if(cache[group_id * CACHE_LINE_NUM + line_rand].dirty == true){
      //mem_write(block_num, cache[group_id * CACHE_LINE_NUM + line_rand].data); //block_num和上次dirty的block_num不一样
      uintptr_t addr_find = (cache[group_id * CACHE_LINE_NUM + line_rand].tag << (INDEX_WIDTH)) | group_id ;
      mem_write(addr_find, cache[group_id * CACHE_LINE_NUM + line_rand].data); 
      cache[group_id * CACHE_LINE_NUM + line_rand].dirty = false;
    }

  #endif

  mem_read(block_num, cache[group_id * CACHE_LINE_NUM + line_rand].data);
  uint32_t *temp  = (void *)(cache[group_id * CACHE_LINE_NUM + line_rand].data) + (block_addr);
  cache[group_id * CACHE_LINE_NUM + line_rand].tag = tag;
  cache[group_id * CACHE_LINE_NUM + line_rand].valid = true;
  *temp = (*temp & ~wmask) | (data & wmask);
  mem_write(block_num, cache[group_id * CACHE_LINE_NUM + line_rand].data); 

  return;
}

// 初始化一个数据大小为 2^total_size_width B，关联度为 2^associativity_width 的 cache
// 例如 init_cache(14, 2) 将初始化一个 16KB，4 路组相联的cache
// 将所有 valid bit 置为无效即可
void init_cache(int total_size_width, int associativity_width) {
  CACHE_LINE_NUM = exp2(associativity_width);

  CACHE_GROUP_NUM = (exp2(total_size_width) / BLOCK_SIZE) / CACHE_LINE_NUM;

  int cache_line_sum = exp2(total_size_width - BLOCK_WIDTH - associativity_width + 2 );

  INDEX_WIDTH =  total_size_width - BLOCK_WIDTH - associativity_width ;

  cache = (cache_block*) malloc (sizeof(cache_block)* cache_line_sum);

  // group1:{cache[0], cache[1], cache[2], cache[3]}
  for(int i = 0; i < cache_line_sum; i++){
    cache[i].valid = false;
    cache[i].dirty = false;
  }
}

void display_statistic(void) {
  puts("Statistic:");
  printf("----------------------------------------\n");
  printf("cycle     : %ld \n", cycle_cnt);
  printf("total_cnt : %ld \n", total_cnt);
  printf("hit       : %ld \n", hit_num);
  printf("miss      : %ld \n", miss_num);
  printf("hit rate  : %.6lf\n", hit_num*1.0/total_cnt);
  printf("hit_read  : %ld \n", hit_read_num);
  printf("hit_write : %ld \n", hit_write_num);
  printf("miss_read : %ld \n", miss_read_num);
  printf("miss_write: %ld \n", miss_write_num);
  printf("----------------------------------------\n");
}

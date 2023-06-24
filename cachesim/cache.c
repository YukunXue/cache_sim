#include "common.h"
#include <inttypes.h>

void mem_read(uintptr_t block_num, uint8_t *buf);
void mem_write(uintptr_t block_num, const uint8_t *buf);

static uint64_t cycle_cnt = 0;

static int CACHE_GROUP_NUM;
static int CACHE_LINE_NUM;
static int INDEX_WIDTH;

void cycle_increase(int n) { cycle_cnt += n; }

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
  uint32_t tag = (addr_valid >>  INDEX_WIDTH); 

  uint32_t block_num = addr >> BLOCK_WIDTH;

  uint32_t *ret;
  bool is_hit = false;

  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid && cache[group_id * CACHE_LINE_NUM + i].tag == tag){
        if(debug){
          printf("read is hit \n");
          printf("err tag the tag is %x \n", tag);
        }

        ret = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        // is_hit = true;
        return *ret;
    }
  }

  for(int i = 0; i < CACHE_LINE_NUM; i++){
    if(cache[group_id * CACHE_LINE_NUM + i].valid == false){
        if(debug){
          printf("read is unhit \n");
        }

        mem_read(block_num, cache[group_id * CACHE_LINE_NUM + i].data);
        cache[group_id * CACHE_LINE_NUM + i].valid = true;
        cache[group_id * CACHE_LINE_NUM + i].tag = tag;
        ret = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        return *ret;
    }
  }


  if(debug){
    printf("???\n");
    //printf("group_id is %d , ")
  }

  int line_rand = rand() % CACHE_LINE_NUM;
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
  uint32_t tag = (addr_valid >> (INDEX_WIDTH)); 

  uint32_t block_num = addr >> BLOCK_WIDTH;

  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid && cache[group_id * CACHE_LINE_NUM + i].tag == tag){

        uint32_t *temp  = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        
        *temp = (*temp & ~wmask) | (data & wmask);
        mem_write(block_num, cache[group_id * CACHE_LINE_NUM + i].data);

        return ;
    }
  }

  for( int i = 0; i < CACHE_LINE_NUM; i++ )
  {
    if(cache[group_id * CACHE_LINE_NUM + i].valid == false){

        mem_read(block_num, cache[group_id * CACHE_LINE_NUM + i].data);

        uint32_t *temp  = (void *)(cache[group_id * CACHE_LINE_NUM + i].data) + (block_addr);
        
        cache[group_id * CACHE_LINE_NUM + i].tag = tag;
        cache[group_id * CACHE_LINE_NUM + i].valid = false;

        *temp = (*temp & ~wmask) | (data & wmask);
        mem_write(block_num, cache[group_id * CACHE_LINE_NUM + i].data);

        return ;
    }
  }

  // printf("here is error while write \n");
  int line_rand = rand() % CACHE_LINE_NUM;
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

  INDEX_WIDTH =  total_size_width - BLOCK_WIDTH - associativity_width  + 2;

  cache = (cache_block*) malloc (sizeof(cache_block)* cache_line_sum);

  // group1:{cache[0], cache[1], cache[2], cache[3]}
  for(int i = 0; i < cache_line_sum; i++){
    cache[i].valid = false;
    cache[i].dirty = false;
  }
}

void display_statistic(void) {
  puts("Statistic:");
}

#include "cache.h"
#include "common.h"
#include "misc.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

uint32_t dram_read(hwaddr_t,size_t);

void dram_write(hwaddr_t,size_t,uint32_t);

uint32_t cachel1_read(uint32_t,uint32_t);

void cachel1_write(uint32_t,uint32_t,uint32_t);

uint32_t cachel2_read(uint32_t,uint32_t);

void cachel2_write(uint32_t,uint32_t,uint32_t);


cachel1 CL1;
cachel2 CL2;

extern double cache_miss_time;
extern double cache_visit_time;

typedef union{
    struct{
        uint32_t blockaddr_bit : CL1_BLOCK_WIDTH;
        uint32_t set_bit : CL1_SET_WIDTH;
        uint32_t tag_bit : CL1_TAG_WIDTH;
    };
    uint32_t addr;
}cachel1_addr;

typedef union{
    struct{
        uint32_t blockaddr_bit : CL2_BLOCK_WIDTH;
        uint32_t set_bit : CL2_SET_WIDTH;
        uint32_t tag_bit : CL2_TAG_WIDTH;
    };
    uint32_t addr;
}cachel2_addr;

void init_CL1(){
    printf("execute init CL1\n");
    int i,j;
    for(i=0;i<CL1_NR_SET;i++){
        for(j=0;j<CL1_NR_WAY;j++){
            CL1.content[i][j].valid=0;
            memset(CL1.content[i][j].data,0,CL1_BLOCK_SIZE);
        }
    }
}

void init_CL2(){
    printf("execute init CL2\n");
    int i,j;
    for(i=0;i<CL2_NR_SET;i++){
        for(j=0;j<CL2_NR_WAY;j++){
            CL2.content[i][j].valid=0;
            CL2.content[i][j].dirty=0;
            memset(CL2.content[i][j].data,0,CL2_BLOCK_SIZE);
        }
    }
}

static int getlen(uint8_t*mask)
{
    uint32_t len=0;
    int k;
    for(k=0;k<CACHEUNIT_LEN;k++){
        if(mask[k]==1)len++;
    }
    return len;
}

static int getoffset(uint8_t*mask)
{
    uint32_t offset=0;
    int k;
    for(k=0;k<CACHEUNIT_LEN;k++){
        if(mask[k]==1){
            offset=k;
            break;
        }
    }
    return offset;
}

/* cache level 1 */
static uint32_t readcl1_miss(uint32_t addr){
    cache_miss_time++;

    cachel1_addr temp;
    temp.addr=addr;
    uint32_t set_bit=temp.set_bit;

    int line;
    for(line=0;line<CL1_NR_WAY;line++){
        if(!CL1.content[set_bit][line].valid)
            break;
    }

    if(line==CL1_NR_WAY){
        srand(time(NULL));
        line=(rand())%CL1_NR_WAY;
    }

    CL1.content[set_bit][line].tag=temp.tag_bit;
    CL1.content[set_bit][line].valid=1;

    uint32_t block_begin=((addr>>CL1_BLOCK_WIDTH)<<CL1_BLOCK_WIDTH);

    int i;
    for(i=0;i<CL1_BLOCK_SIZE;i++){
        CL1.content[set_bit][line].data[i]=(cachel2_read(block_begin+i,1)&0xff);
    }

    return line;

}

static void cl1unit_read(uint32_t addr,void* data){
    cache_visit_time++;
    cachel1_addr temp;
    temp.addr=addr& ~CACHEUNIT_MASK;
    uint32_t blockaddr_bit=temp.blockaddr_bit;
    uint32_t set_bit=temp.set_bit;
    uint32_t tag_bit=temp.tag_bit;

    int line;
    for(line=0;line<CL1_NR_WAY;line++){
        if(CL1.content[set_bit][line].valid&&CL1.content[set_bit][line].tag==tag_bit)
            break;
    }

    if(line==CL1_NR_WAY){
        line=readcl1_miss(addr);
    }

    memcpy(data,CL1.content[set_bit][line].data+blockaddr_bit,CACHEUNIT_LEN);
}

uint32_t cachel1_read(uint32_t addr,uint32_t len){
    uint32_t offset=addr&CACHEUNIT_MASK;

    uint8_t temp[2*CACHEUNIT_LEN];
    memset(temp,0,sizeof(temp));

    cl1unit_read(addr,temp);

    if(offset+len>CACHEUNIT_LEN){
        cl1unit_read(addr+4,temp+4);
    }

    return unalign_rw(temp+offset,4);
}

static void cl1unit_write(uint32_t addr,uint8_t*data,uint8_t*mask,uint32_t len,uint32_t offset){
    cache_visit_time++;

    cachel1_addr temp;
    temp.addr=addr& ~CACHEUNIT_MASK;
    uint32_t blockaddr_bit=temp.blockaddr_bit;
    uint32_t set_bit=temp.set_bit;
    uint32_t tag_bit=temp.tag_bit;

    int line;
    for(line=0;line<CL1_NR_WAY;line++){
        if((CL1.content[set_bit][line].valid)&&(CL1.content[set_bit][line].tag==tag_bit))
            break;
    }

    if(line==CL1_NR_WAY){
        cachel2_write((addr&(~CACHEUNIT_MASK))+offset, len, unalign_rw(data+offset,4));
    }
    else{
        memcpy_with_mask(CL1.content[set_bit][line].data + blockaddr_bit, data, CACHEUNIT_LEN, mask);
        cachel2_write((addr&(~CACHEUNIT_MASK))+offset,len,unalign_rw(data+offset,4));
    }
}

void cachel1_write(uint32_t addr,uint32_t len,uint32_t data){
    uint32_t offset=addr&CACHEUNIT_MASK;
    uint8_t temp[2*CACHEUNIT_LEN];
    uint8_t mask[2*CACHEUNIT_LEN];
    uint32_t masklen,maskoffset;

    memset(mask,0,2*CACHEUNIT_LEN);

    *(uint32_t *)(temp+offset)=data;
    memset(mask+offset,1,len);

    masklen=getlen(mask);
    maskoffset=getoffset(mask);

    cl1unit_write(addr,temp,mask,masklen,maskoffset);

    if((offset+len)>CACHEUNIT_LEN){
        masklen=getlen(mask+CACHEUNIT_LEN);
        maskoffset=getoffset(mask+CACHEUNIT_LEN);
        cl1unit_write(addr+CACHEUNIT_LEN,temp+CACHEUNIT_LEN,mask+CACHEUNIT_LEN,masklen,maskoffset);
    }
}

/* cache level 2 */

static uint32_t readcl2_miss(uint32_t addr){
    cache_miss_time++;

    cachel2_addr temp;
    temp.addr=addr;
    uint32_t set_bit=temp.set_bit;

    int line;
    for(line=0;line<CL2_NR_WAY;line++){
        if(!CL2.content[set_bit][line].valid)
            break;
    }

    if(line==CL2_NR_WAY){
        srand(time(NULL));
        line=(rand())%CL2_NR_WAY;

        if(CL2.content[set_bit][line].dirty){
            int i;
            uint32_t addr_begin=((CL2.content[set_bit][line].tag)<<(CL2_BLOCK_WIDTH+CL2_SET_WIDTH))+(set_bit<<(CL2_BLOCK_WIDTH));
            for(i=0;i<CL2_BLOCK_SIZE;i++){
                dram_write(addr_begin+i,1,CL2.content[set_bit][line].data[i]);
            }
        }

        CL2.content[set_bit][line].dirty=0;
    }

    CL2.content[set_bit][line].tag=temp.tag_bit;
    CL2.content[set_bit][line].valid=1;

    uint32_t block_begin=((addr>>CL2_BLOCK_WIDTH)<<CL2_BLOCK_WIDTH);

    int i;
    for(i=0;i<CL2_BLOCK_SIZE;i++){
        CL2.content[set_bit][line].data[i]=(dram_read(block_begin+i,1)&0xff);
    }

    return line;

}

static void cl2unit_read(uint32_t addr,void* data){
    cache_visit_time++;
    cachel2_addr temp;
    temp.addr=addr& ~CACHEUNIT_MASK;
    uint32_t blockaddr_bit=temp.blockaddr_bit;
    uint32_t set_bit=temp.set_bit;
    uint32_t tag_bit=temp.tag_bit;

    int line;
    for(line=0;line<CL2_NR_WAY;line++){
        if(CL2.content[set_bit][line].valid&&CL2.content[set_bit][line].tag==tag_bit)
            break;
    }

    if(line==CL2_NR_WAY){
        line=readcl2_miss(addr);
    }

    memcpy(data,CL2.content[set_bit][line].data+blockaddr_bit,CACHEUNIT_LEN);
}

uint32_t cachel2_read(uint32_t addr,uint32_t len){
    uint32_t offset=addr&CACHEUNIT_MASK;

    uint8_t temp[2*CACHEUNIT_LEN];
    memset(temp,0,sizeof(temp));

    cl2unit_read(addr,temp);

    if(offset+len>CACHEUNIT_LEN){
        cl2unit_read(addr+4,temp+4);
    }

    return unalign_rw(temp+offset,4);

}

static void cl2unit_write(uint32_t addr,uint8_t*data,uint8_t*mask,uint32_t len,uint32_t offset){
    cache_visit_time++;

    cachel2_addr temp;
    temp.addr=addr& ~CACHEUNIT_MASK;
    uint32_t blockaddr_bit=temp.blockaddr_bit;
    uint32_t set_bit=temp.set_bit;
    uint32_t tag_bit=temp.tag_bit;

    int line;
    for(line=0;line<CL2_NR_WAY;line++){
        if((CL2.content[set_bit][line].valid)&&(CL2.content[set_bit][line].tag==tag_bit))
            break;
    }

    if(line==CL2_NR_WAY){
        dram_write((addr&(~CACHEUNIT_MASK)) + offset, len, unalign_rw(data+offset,4));
        readcl2_miss(addr);
    }
    else{
        memcpy_with_mask(CL2.content[set_bit][line].data + blockaddr_bit, data, CACHEUNIT_LEN, mask);
        CL2.content[set_bit][line].dirty=1;
    }
}

void cachel2_write(uint32_t addr,uint32_t len,uint32_t data){
    uint32_t offset=addr&CACHEUNIT_MASK;
    uint8_t temp[2*CACHEUNIT_LEN];
    uint8_t mask[2*CACHEUNIT_LEN];
    uint32_t masklen,maskoffset;
    memset(mask,0,2*CACHEUNIT_LEN);

    *(uint32_t *)(temp+offset)=data;
    memset(mask+offset,1,len);

    masklen=getlen(mask);
    maskoffset=getoffset(mask);
    cl2unit_write(addr,temp,mask,masklen,maskoffset);

    if((offset+len)>CACHEUNIT_LEN){
        masklen=getlen(mask+CACHEUNIT_LEN);
        maskoffset=getoffset(mask+CACHEUNIT_LEN);
        cl2unit_write(addr+CACHEUNIT_LEN,temp+CACHEUNIT_LEN,mask+CACHEUNIT_LEN,masklen,maskoffset);
    }

}

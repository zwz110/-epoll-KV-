#pragma once
#include<stddef.h>
#include<stdint.h>

struct HNode{
    HNode*next=NULL;
    uint64_t hcode;     //哈希值
};
struct Htab{
    HNode**tab=NULL;
    size_t msg=0;   //数组中的键数-1
    size_t size=0;  //哈希表中已存入的键值对
};
struct Hmap{
    Htab older;
    Htab newer;
    size_t migrate_pos=0;
};
HNode*hm_lookup(Hmap*hmap,HNode*key,bool(*eq)(HNode*,HNode*));
void hm_insert(Hmap*hmap,HNode*node);
HNode*hm_delete(Hmap*hmap,HNode*key,bool(*eq)(HNode*,HNode*));
void hm_clear(Hmap*hmap);
size_t hm_size(Hmap*hmap);
// invoke the callback on each node until it returns false
void hm_foreach(Hmap*hmap,bool(*f)(HNode*,void*),void*args);
void hm_init(Hmap*hmap);
#include"hashtable.h"
#include<stdlib.h>
#include<cassert>

static void h_init(Htab*htab,size_t n){ 
    assert(n>0&&((n-1)&n)==0);
    htab->tab=(HNode**)calloc(n,sizeof(HNode*));
    htab->size=0;
    htab->msg=n-1; //n必须是2的幂次方
}

static HNode**h_lookup(Htab*htab,HNode*key,bool(*eq)(HNode*,HNode*)){
    if(!htab || !htab->tab)   return NULL;
    size_t pos=key->hcode&htab->msg;
    HNode**from=&htab->tab[pos];
    for(HNode*cur;(cur=*from)!=NULL;from=&cur->next){
        //用 from 这个局部变量来存储指针的指针，从而直接修改from变量的值，不对链表有任何影响
        //必须用from=&cur->next修改这个局部变量本身，
        //用*from=cur->next会修改链表结构，这会操作链表,严重错误
        if(key->hcode==cur->hcode&&eq(cur,key))
            return from;    //返回目标节点指针的指针，便于删除函数利用.
    }
    return NULL;
}
static void h_insert(Htab*htab,HNode*key){
    size_t pos=key->hcode&htab->msg;
    HNode*from=htab->tab[pos];  //from是一个临时变量，不是链表结构的一部分
    key->next=from;
    htab->tab[pos]=key;
    htab->size++;
}

static HNode*h_detach(Htab*htab,HNode**from){
    HNode*node=*from;
    *from=node->next;
    htab->size--;
    return node;
}

const size_t k_rehashing_num=128;
static void help_rehashing(Hmap*hmap){
    size_t worked=0;
    while(hmap->older.size>0&&worked<k_rehashing_num){
        HNode**from=&hmap->older.tab[hmap->migrate_pos];
        if(!(*from)){
            hmap->migrate_pos++;
            continue;
        }
        h_insert(&hmap->newer,h_detach(&hmap->older,from));
        worked++;
    }
    if(hmap->older.size==0&&hmap->older.tab){
        free(hmap->older.tab);
        hmap->older=Htab{};
    }
}
static void hm_trigger_rehashing(Hmap*hmap){
    assert(hmap->older.tab==NULL);          //在开始新的rehash之前，必须确保没有rehash正在进行
    hmap->older=hmap->newer;
    h_init(&hmap->newer,(hmap->newer.msg+1)*2);//更新为原来的二倍
    hmap->migrate_pos=0;
}
HNode*hm_lookup(Hmap*hmap,HNode*key,bool(*eq)(HNode*,HNode*)){
    help_rehashing(hmap);
    HNode**from=h_lookup(&hmap->newer,key,eq);
    if(!from)
        from=h_lookup(&hmap->older,key,eq);
    return from?*from:NULL;
}

const size_t k_max_load_factor=8;
void hm_insert(Hmap*hmap,HNode*node){
    if(!hmap->newer.tab){
        h_init(&hmap->newer,4);
    }
    assert(node);
    h_insert(&hmap->newer,node);
    if(!hmap->older.tab){//确保没有rehash正在进行
        size_t ren=(hmap->newer.msg+1)*k_max_load_factor;
        if(ren<=hmap->newer.size){
            hm_trigger_rehashing(hmap);
        }
    }
    help_rehashing(hmap);
}
HNode*hm_delete(Hmap*hmap,HNode*key,bool(*eq)(HNode*,HNode*)){
    help_rehashing(hmap);
    if(HNode**from=h_lookup(&hmap->newer,key,eq))
        return  h_detach(&hmap->newer,from);
    if(HNode**from=h_lookup(&hmap->older,key,eq))
        return  h_detach(&hmap->older,from);
    return NULL;
}
size_t hm_size(Hmap*hmap){
    return hmap->newer.size+hmap->older.size;
}
void hm_clear(Hmap*hmap){
    free(hmap->newer.tab);
    free(hmap->older.tab);
    *hmap=Hmap{};//hmap = &HMap{};是临时变量的地址
}

static bool h_foreach(Htab*htab,bool(*f)(HNode*,void*),void*args){
    for(size_t i=0;htab->msg!=0&&i<htab->msg;++i){  // 遍历每个槽位
        for(HNode*node=htab->tab[i];node!=NULL;node=node->next){    // 遍历每个槽位的链表
            if(!f(node,args)){
                return false;
            }
        }
    }
    return true;
}
void hm_foreach(Hmap*hmap,bool(*f)(HNode*,void*),void*args){
    h_foreach(&hmap->newer, f, args) && h_foreach(&hmap->older, f, args);
}
void hm_init(Hmap*hmap){
    if(!hmap->newer.tab){
        h_init(&hmap->newer,4);
    }
}
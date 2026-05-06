#pragma once

#include<stddef.h>
#include<stdint.h>

struct AVLNode{
    AVLNode*parent;
    AVLNode*left;
    AVLNode*right;
    uint32_t height;    //高度
    uint32_t size;      //子树节点个数
};

//inline 函数（内联函数）:
//    调用处直接把函数代码粘贴过去，没有跳转,速度更快

inline void avl_init(AVLNode*node){
    node->left=node->parent=node->right=NULL;
    node->height=1;
    node->size=1;
}
inline uint32_t avl_height(AVLNode*node){
    return node?node->height:0;
}
inline uint32_t avl_size(AVLNode*node){
    return node?node->size:0;
}

//函数
AVLNode*avl_fix(AVLNode*node);
AVLNode*avl_del(AVLNode*node);
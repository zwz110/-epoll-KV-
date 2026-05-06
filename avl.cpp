#include"avl.h"
#include<assert.h>

static uint32_t cur_max(uint32_t a,uint32_t b){
    return a>b?a:b;
}
//保留高度和 size字段
static void avl_update(AVLNode*node){
    node->height=1+cur_max(avl_height(node->left),avl_height(node->right));
    node->size=1+avl_size(node->left)+avl_size(node->right);
}

//左旋,未更新parent节点的左或者右的指针指向
static AVLNode*rot_left(AVLNode*node){
    AVLNode*parent=node->parent;
    AVLNode*new_node=node->right;
    AVLNode*inner=new_node->left;
    node->right=inner;
    node->parent=new_node;
    if(inner){
        inner->parent=node;
    }
    new_node->left=node;
    new_node->parent=parent;
    avl_update(node);
    avl_update(new_node);
    return new_node;
}
//左旋,未更新parent节点的左或者右的指针指向
static AVLNode*rot_right(AVLNode*node){
    AVLNode*parent=node->parent;
    AVLNode*new_node=node->left;
    AVLNode*inner=new_node->right;

    node->left=inner;
    node->parent=new_node;
    if(inner)
        inner->parent=node;
    new_node->right=node;
    new_node->parent=parent;
    avl_update(node);
    avl_update(new_node);
    return new_node;
}

//修正LL和LR失衡
static AVLNode*avl_fix_left(AVLNode*node){
    if(avl_height(node->left->left)<avl_height(node->left->right)){
        node->left=rot_left(node->left);
    }
    return rot_right(node);
}


//修正RR和RL失衡
static AVLNode*avl_fix_right(AVLNode*node){
    if(avl_height(node->right->right)<avl_height(node->right->left)){
        node->right=rot_right(node->right);
    }
    return rot_left(node);
}

AVLNode*avl_fix(AVLNode*node){
    while(true){
        AVLNode**from=&node;
        AVLNode*parent=node->parent;
        if(parent){
            from=parent->left==node?&parent->left:&parent->right;
        }
        avl_update(node);//确保高度信息是最新的,处理完当前节点后，若节点发生旋转，新的子树根节点高度已被旋转函数更新。
        uint32_t l=avl_height(node->left);
        uint32_t r=avl_height(node->right);
        if(l==r+2){
            *from=avl_fix_left(node);
        }
        else if(l+2==r){
            *from=avl_fix_right(node);
        }
        if(!parent)
            return *from;
        node=parent;
    }
}

//删除的节点只有0或者1个子节点
static AVLNode* avl_easy_del(AVLNode*node){
    assert(!node->left || !node->right);
    AVLNode*child=node->left?node->left:node->right;
    AVLNode*parent=node->parent;
    if(child){
        child->parent=parent;
    }
    if(!parent){
        return child;
    }
    AVLNode**from=parent->left==node?&parent->left:&parent->right;
    *from=child;
    return avl_fix(parent);
}

AVLNode*avl_del(AVLNode*node){
    if(!node->left || !node->right){
        return avl_easy_del(node);
    }
    //把node的后继节点移到当前位置 
    AVLNode*victim=node->right;
    while(victim->left){
        victim=victim->left;
    }
    AVLNode*root=avl_easy_del(victim);
    //victim 节点变成了原来 node 的完全克隆，包括高度、左右孩子指针、父指针等所有字段。
    *victim=*node;

    //由于我们整体复制了 node 的 left 和 right 指针，这两个指针现在指向原来的左右子树。
    //但这些子树内部的节点的 parent 指针仍然指向旧的 node 地址。
    if(victim->left){
        victim->left->parent=victim;
    }
    if(victim->right){
        victim->right->parent=victim;
    }
    AVLNode*parent=node->parent;
    AVLNode**from=&root;
    if(parent){
        from=parent->left==node?&parent->left:&parent->right;
    }
    *from=victim;
    return root;
}
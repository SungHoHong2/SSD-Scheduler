#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2

typedef struct node {
    int data ;
    int start_tag;
    int finish_tag;
} node;

typedef struct minHeap {
    int size ;
    node **elem_test;
} minHeap;

void swap_test(node *n1, node *n2) {
    node temp = *n1 ;
    *n1 = *n2 ;
    *n2 = temp ;
}

void heapify(minHeap *hp, int i) {
    int smallest = (LCHILD(i) < hp->size && hp->elem_test[LCHILD(i)]->data < hp->elem_test[i]->data) ? LCHILD(i) : i ;
    if(RCHILD(i) < hp->size && hp->elem_test[RCHILD(i)]->data < hp->elem_test[smallest]->data) {
        smallest = RCHILD(i) ;
    }
    if(smallest != i) {
        swap_test(hp->elem_test[i], hp->elem_test[smallest]) ;
        heapify(hp, smallest) ;
    }
}

void insertNode(minHeap *hp, node *nd) {
   int i;
    if(hp->size) {
        hp->elem_test = (node **)krealloc(hp->elem_test, (hp->size + 1) * sizeof(node *), GFP_KERNEL) ;
    } else {
        hp->elem_test = (node **)kmalloc(sizeof(node *), GFP_KERNEL) ;
    }

        i = (hp->size)++ ;
    while(i && nd->data < hp->elem_test[PARENT(i)]->data) {
        hp->elem_test[i] = hp->elem_test[PARENT(i)] ;
        i = PARENT(i) ;
    }
    hp->elem_test[i] = nd ;
}

void deleteNode(minHeap *hp) {
    if(hp->size) {
        printk("Deleting node %d\n\n", hp->elem_test[0]->data) ;
        hp->elem_test[0] = hp->elem_test[--(hp->size)] ;
        hp->elem_test = (node **)krealloc(hp->elem_test, hp->size * sizeof(node *), GFP_KERNEL) ;
        heapify(hp, 0) ;
    } else {
        kfree(hp->elem_test) ;
    }
}

int getMaxNode(minHeap *hp, int i) {
    int l, r;
    if(LCHILD(i) >= hp->size) {
        return hp->elem_test[i]->data ;
    }

    l = getMaxNode(hp, LCHILD(i)) ;
    r = getMaxNode(hp, RCHILD(i)) ;

    if(l >= r) {
        return l ;
    } else {
        return r ;
    }
}



static int __init sfq_init(void){
  minHeap *hp;
  node *n;
  int i;

  hp =  (minHeap *)kmalloc(sizeof(minHeap), GFP_KERNEL) ;
  hp->size=0;

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 9;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
   n->data = 2;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 0;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 3;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 1;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 8;
  insertNode(hp, n);

  n = (node *)kmalloc(sizeof(node), GFP_KERNEL) ;
  n->data = 3;
  insertNode(hp, n);

  printk("maximum test : %d",getMaxNode(hp,0));

  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node


  for(i = 0; i < hp->size; i++) {
      printk("%d ", hp->elem_test[i]->data) ;
  }

  printk("howdyhowdy\n");

  return 0;
}

static void __exit sfq_exit(void){
}

module_init(sfq_init);
module_exit(sfq_exit);

MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SFQ experiment");

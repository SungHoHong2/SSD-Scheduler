#include <stdio.h>
#include <stdlib.h>

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2


struct node {
    int data ;
    int start_tag;
    int finish_tag;
};

struct minHeap {
    int size ;
    node **elem_test;
};

void swap(node *n1, node *n2) {
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
        swap(hp->elem_test[i], hp->elem_test[smallest]) ;
        heapify(hp, smallest) ;
    }
}

void insertNode(minHeap *hp, node *nd) {
    if(hp->size) {
        hp->elem_test = (node **)realloc(hp->elem_test, (hp->size + 1) * sizeof(node *)) ;
    } else {
        hp->elem_test = (node **)malloc(sizeof(node *)) ;
    }

    int i = (hp->size)++ ;
    while(i && nd->data < hp->elem_test[PARENT(i)]->data) {
        hp->elem_test[i] = hp->elem_test[PARENT(i)] ;
        i = PARENT(i) ;
    }
    hp->elem_test[i] = nd ;
}

void deleteNode(minHeap *hp) {
    if(hp->size) {
        printf("Deleting node %d\n\n", hp->elem_test[0]->data) ;
        hp->elem_test[0] = hp->elem_test[--(hp->size)] ;
        hp->elem_test = (node **)realloc(hp->elem_test, hp->size * sizeof(node *)) ;
        heapify(hp, 0) ;
    } else {
        free(hp->elem_test) ;
    }
}

int getMaxNode(minHeap *hp, int i) {
    if(LCHILD(i) >= hp->size) {
        return hp->elem_test[i]->data ;
    }

    int l = getMaxNode(hp, LCHILD(i)) ;
    int r = getMaxNode(hp, RCHILD(i)) ;

    if(l >= r) {
        return l ;
    } else {
        return r ;
    }
}


int main(void){

  minHeap *hp =  (minHeap *)malloc(sizeof(minHeap)) ;
  hp->size=0;

  node *n = (node *)malloc(sizeof(node)) ;
  n->data = 9;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
   n->data = 2;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
  n->data = 0;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
  n->data = 3;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
  n->data = 1;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
  n->data = 8;
  insertNode(hp, n);

  n = (node *)malloc(sizeof(node)) ;
  n->data = 3;
  insertNode(hp, n);

  printf("maximum test : %d",getMaxNode(hp,0));

  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node


  for(int i = 0; i < hp->size; i++) {
      printf("%d ", hp->elem_test[i]->data) ;
  }

  printf("howdyhowdy\n");

  return 0;
}

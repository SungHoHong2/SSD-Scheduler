#ifndef MINHEAP_H_
#define MINHEAP_H_

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
#define PARENT(x) (x - 1) / 2

typedef struct node {
    int data ;
    int start_tag;

} node ;

typedef struct minHeap {
    int size ;
    node *elem ;

    node **elem_test;
} minHeap ;


minHeap initMinHeap();

void swap(node *n1, node *n2);

void heapify(minHeap *hp, int i);

void buildMinHeap(minHeap *hp, int *arr, int size);

void insertNode(minHeap *hp, node *data);
// void insertNode(minHeap *hp, int data);

void deleteNode(minHeap *hp);

int getMaxNode(minHeap *hp, int i);

void deleteMinHeap(minHeap *hp);

void inorderTraversal(minHeap *hp, int i);

void preorderTraversal(minHeap *hp, int i);

void postorderTraversal(minHeap *hp, int i);

void levelorderTraversal(minHeap *hp);

#endif

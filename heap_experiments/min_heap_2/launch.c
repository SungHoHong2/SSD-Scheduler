#include <stdio.h>
#include <stdlib.h>
#include "minHeap.h"


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

  int i ;
  for(i = 0; i < hp->size; i++) {
      printf("%d ", hp->elem_test[i]->data) ;
  }

  printf("\n");


  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node
  deleteNode(hp); //remove the smallest node


  for(i = 0; i < hp->size; i++) {
      printf("%d ", hp->elem_test[i]->data) ;
  }

  printf("howdyhowdy\n");

  return 0;
}

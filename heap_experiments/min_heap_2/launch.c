#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "minHeap.h"


int main(void){

  clock_t start = clock();

  minHeap *hp =  (minHeap *)malloc(sizeof(minHeap)) ;
  hp->size=0;


  // int i;
  // for(i=1; i<=100; i++){
  //   node *n = (node *)malloc(sizeof(node)) ;
  //   n->data = i;
  //   insertNode(hp, n);
  // }
  //
  // while(hp->size!=0){
  //   deleteNode(hp); //remove the smallest node
  // }


  // int i;
  // for(i=100000; i>=1; i--){
  //   node *n = (node *)malloc(sizeof(node)) ;
  //   n->data = i;
  //   insertNode(hp, n);
  // }
  //
  // while(hp->size!=0){
  //   deleteNode(hp); //remove the smallest node
  // }


  int i;
  int index = 10;

  for(i=1; i<=index; i++){
    node *n = (node *)malloc(sizeof(node)) ;
    n->data = (rand()%index)+2;
    insertNode(hp, n);
  }

  while(hp->size!=0){
    deleteNode(hp); //remove the smallest node
  }


  clock_t end = clock();
  float seconds = (float)(end - start) / CLOCKS_PER_SEC;
  printf("duration time %f\n", seconds);



  return 0;
}

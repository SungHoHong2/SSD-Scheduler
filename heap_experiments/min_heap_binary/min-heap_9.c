/*

 Minimum Heap tree
 Store elemnts for quick retrieval in sorted order

 Parent is always less in value than it's two children

 each time something when something is added, it has to be checked
 so it is in order.


 * it has to be a complete tree,
   - all levels are full, and last level has no gaps on the left.


 * delete (don't need tracking need to remember the last node)
   - only need remove the root
   - switch the root with the last node
   - heapify

* insert (need tracking to get find the location to add)
   - adds from the left
   - the added node should be the last node
   - heapify

*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdlib.h>

#define PARENT(x) (x - 1) / 2

struct node{
  int data;
  struct node *left;
  struct node *right;
  struct node *parent;
};

struct track_node{
  int path;
  struct track_node *right;
};


// keep track of total number of nodes
static int total_nodes;
struct node *latest_node;

void swap(struct node *n1, struct node *n2) {
    int temp;

    // printf("\t\tswapped: n1: %d  n2: %d \n", n1->data, n2->data);

    temp = n1->data;
    n1->data = n2->data;
    n2->data = temp;

    // printf("\t\tswapped: n1: %d  n2: %d \n", n1->data, n2->data);

}


// CHARA
void heapify_upward(struct node *selected_node){
   struct node *smallest;

   if(!selected_node) goto skip_heapify;

   if(selected_node->parent) selected_node = selected_node->parent;

    // printf("\t\tselected_node %d\n",selected_node->data);
    // if(selected_node->left) printf("\t\tselected_node left %d\n",selected_node->left->data);
    // if(selected_node->right) printf("\t\tselected_node right %d\n",selected_node->right->data);

   if(selected_node->left && (selected_node->left->data<selected_node->data)){
        smallest = selected_node->left;
   }

   if(smallest && (selected_node->right) && (selected_node->right->data<smallest->data)){
        smallest = selected_node->right;
   } else if((selected_node->right) && (selected_node->right->data<selected_node->data)){
        smallest = selected_node->right;
   }


    //  printf("heapify upward\n");
   if(smallest && (smallest->data != selected_node->data)){
      // printf("\t\tswap before:  selected: %d  smallest: %d  \n", selected_node->data, smallest->data);

      if(selected_node->data > smallest->data){
       swap(smallest, selected_node);
    //  printf("\t\tswap after:  selected: %d  smallest: %d  \n", selected_node->data, smallest->data);
     // printf("");
       heapify_upward(selected_node);
      }
   }

   skip_heapify: ;

}


void heapify_downward(struct node *selected_node){
   struct node *smallest;
   int s = 0;


   if(!selected_node) goto skip_heapify;

   smallest = selected_node;

  //  printf("selected_node: %d  \n", smallest->data);

   if(selected_node->left && selected_node->left->data && (selected_node->left->data<=smallest->data)){
        smallest = selected_node->left;
            // printf("selected_node left %d  \n", selected_node->left->data);
   }

   if(smallest && (selected_node->right) && (selected_node->right->data<=smallest->data)){
        smallest = selected_node->right;
          // printf("selected_node right %d  \n", selected_node->right->data);
   }

    // printf("heapify downward\n");
   if(smallest && (smallest->data) && (smallest->data != selected_node->data)){
      // printf("\t\tswap before:  selected: %d  smallest: %d  \n", selected_node->data, smallest->data);
      swap(smallest, selected_node);
      // printf("\t\tswap after:  selected: %d  smallest: %d  \n", selected_node->data, smallest->data);
     heapify_downward(smallest);
   }

   skip_heapify: ;

}


struct node* get_new_node(int data){
  struct node *new_node = (struct node *) malloc(sizeof(struct node));
  new_node->data = data;
  new_node->left = new_node->right = new_node->parent = NULL;

  total_nodes++; // increase number of nodes

  // printf("total_nodes %d\n", total_nodes);
  return new_node;
}


struct track_node* tracking_nodes(int total_nodes){

  int target_index;
  struct track_node *target_path;
  struct track_node *temp;
  target_index = total_nodes;

  target_path = (struct track_node *) malloc(sizeof(struct track_node));
  target_path->path = target_index%2;
  target_path->right = NULL;


  //  if(target_path->path) printf("target_path left\n");
  //  else printf("target_path right\n");


  target_index = PARENT(target_index);
  while(target_index!=0){
     // printf("parent exist\n");
     temp = target_path;
     target_path = (struct track_node *) malloc(sizeof(struct track_node));
     target_path->path = target_index%2;
     target_path->right = temp;


      // if(target_path->path) printf("target_path left\n");
      // else printf("target_path right\n");

     target_index = PARENT(target_index);
  }


  temp = target_path;
  target_path = (struct track_node *) malloc(sizeof(struct track_node));
  target_path->path = target_index%2;
  target_path->right = temp;

  // printf("root_path %d\n", target_path->path);
  return target_path;
}




void insert_node(struct node *root, int data){
    struct track_node *td;
    struct track_node *temp;
    struct node *new_node;
    struct node *child_node;

    new_node = get_new_node(data);
    child_node = root;
    td = tracking_nodes(total_nodes);

    //  printf("insert_node\n");
    while(td->right){
      temp = td;
      td = td->right;
      free(temp);

      if(td->path){
          // printf("\t\ttarget_path left\n");
          if(!(child_node->left)){
              child_node->left = new_node;
              new_node->parent = child_node;
          }
          child_node = child_node->left;
      }else{
          // printf("\t\ttarget_path right\n");
          if(!(child_node->right)){
              child_node->right = new_node;
              new_node->parent = child_node;
          }
          child_node = child_node->right;
      }
          // printf("\t\tthe data: %d\n", child_node->data);
    }

    heapify_upward(child_node);
    latest_node = child_node;
}



void refresh_latest_node(struct node *root){

  struct track_node *td;
  struct track_node *temp;
  struct node *child_node;

  child_node = root;
  td = tracking_nodes(total_nodes);

  // printf("refresh_latest_node  tracking_nodes %d \n",total_nodes );
  // printf("refresh_latest_node\n");
  while(td->right){
    temp = td;
    td = td->right;
    free(temp);

    if(td->path){
        // printf("target_path left\n");
        if(child_node->left){
              child_node = child_node->left;
        }
    }else{
        // printf("target_path right\n");
        if(child_node->right){
              child_node = child_node->right;
        }
    }

  }

  // printf("latest_node : %d\n", child_node->data);
  latest_node = child_node;
  latest_node->left = latest_node->right = NULL;
}

int swap_and_delete(struct node *root, struct node *latest_node){
  int data;
  int rtn = root->data;

  data = latest_node->data;
  root->data = data;


  if(latest_node->parent){
      if(total_nodes%2) latest_node->parent->left = NULL;
      else  latest_node->parent->right = NULL;
  }

  latest_node = NULL;
  total_nodes--;

  // printf("swap_and_delete\n");
  // printf("\t\troot %d \n", root->data);

  return rtn;
}


int root_entry(struct node *root){
  struct node *temp = root;
  int rtn;

  // printf("\t\troot %d latest_node %d\n", root->data, latest_node->data);
  rtn = swap_and_delete(root, latest_node);
  heapify_downward(root);

  return rtn;
}



int main(){

  clock_t start = clock();

  struct node *root;
  int rtn;
  total_nodes = -1;

  // BEST CASE




  // WORST CASE
  // root = get_new_node(100000);
  //
  // int i;
  // for(i=99999; i>=1; i--){
  //   insert_node(root, i);
  // }
  //  while(total_nodes!=0){
  //        rtn = root_entry(root);
  //        refresh_latest_node(root);
  //        printf("data taken out: %d\n", rtn);
  //  }




  // int index = 13;
  // int array[] = {12,10,6,10,8,13,9,11,2,2,15,5,8,8};
  //
  // root = get_new_node(array[0]);
  // printf("first root data: %d\n", root->data);
  //
  // int i;
  // for(i=1; i<index; i++){
  //   insert_node(root, array[i]);
  // }
  //  while(total_nodes!=0){
  //        rtn = root_entry(root);
  //        refresh_latest_node(root);
  //        printf("data taken out: %d\n", rtn);
  //  }
  //  if(total_nodes==0)
  //  printf("final data taken out: %d\n", root->data);


  // int index = 10000;
  // srand(time(NULL));
  // root = get_new_node((rand()%index)+1);
  // // printf("data added in: %d\n", root->data);
  //
  // int i;
  // int s;
  // for(i=2; i<=index; i++){
  //   s =(rand()%index)+1;
  //   // printf("data added in: %d\n", s);
  //   insert_node(root, s);
  // }
  //  while(total_nodes!=0){
  //        rtn = root_entry(root);
  //        refresh_latest_node(root);
  //        printf("data taken out: %d\n", rtn);
  //  }

  clock_t end = clock();
  float seconds = (float)(end - start) / CLOCKS_PER_SEC;
  printf("duration time %f\n", seconds);




  return 0;
}

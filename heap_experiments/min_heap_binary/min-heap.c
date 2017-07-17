/*

 Minimum Heap tree
 Store elemnts for quick retrieval in sorted order

 Parent is always less in value than it's two children

 each time something when something is added, it has to be checked
 so it is in order.


 * it has to be a complete tree,
   - all levels are full, and last level has no gaps on the left.


 * search + delete (don't need tracking need to remember the last node)
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

#define LCHILD(x) 2 * x + 1
#define RCHILD(x) 2 * x + 2
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
struct node *rtn_node;


void swap(struct node *n1, struct node *n2) {
    int temp;
    temp = n1->data;
    n1->data = n2->data;
    n2->data = temp;
}


void heapify_upward(struct node *selected_node){
   struct node *smallest;
   int s = 0;


   if(!selected_node) goto skip_heapify;

   printf("---->heapify BEGIN: %d \n", s++);
   printf("selected_node data: %d \n", selected_node->data);


   if(!(selected_node->parent)) goto skip_heapify;

   selected_node = selected_node->parent;

   // Chara
   // you need to compare all of the childs and choose the smallest

   if(selected_node->left && (selected_node->left->data<=selected_node->data)){
        smallest = selected_node->left;
   }else if((selected_node->right) && (selected_node->right->data<=selected_node->data)){
        smallest = selected_node->right;
   }


   if(smallest && (smallest->data != selected_node->data)){
     printf("---->heapify swap before: %d  selected: %d  smallest: %d  \n", s++, selected_node->data, smallest->data);
     swap(smallest, selected_node);
     printf("---->heapify swap after: %d  selected: %d  smallest: %d  \n", s++, selected_node->data, smallest->data);
     heapify_upward(selected_node);
   }
     printf("---->heapify END: %d \n", s++);

   skip_heapify: ;

}


void heapify_downward(struct node *selected_node){
   struct node *smallest;
   int s = 0;

   if(!selected_node) goto skip_heapify;


   if(selected_node->left && (selected_node->left->data<=selected_node->data)){
        smallest = selected_node->left;

        // Chara
        // you need to compare all of the childs and choose the smallest


   }else if((selected_node->right) && (selected_node->right->data<=selected_node->data)){
        smallest = selected_node->right;
   }

     printf("---->heapify BEGIN: %d \n", s++);

   if(smallest && (smallest->data != selected_node->data)){
     printf("---->heapify swap before: %d  selected: %d  smallest: %d  \n", s++, selected_node->data, smallest->data);
     swap(smallest, selected_node);
     printf("---->heapify swap after: %d  selected: %d  smallest: %d  \n", s++, selected_node->data, smallest->data);
     heapify_downward(smallest);
   }

     printf("---->heapify END: %d \n", s++);

   skip_heapify: ;

}


struct node* get_new_node(int data){
  struct node *new_node = (struct node *) malloc(sizeof(struct node));
  new_node->data = data;
  new_node->left = new_node->right = new_node->parent = NULL;

  total_nodes++; // increase number of nodes
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


  // if(target_path->path) printf("target_path left\n");
  // else printf("target_path right\n");


  target_index = PARENT(target_index);
  while(target_index!=0){
     // printf("parent exist\n");
     temp = target_path;
     target_path = (struct track_node *) malloc(sizeof(struct track_node));
     target_path->path = target_index%2;
     target_path->right = temp;


    //  if(target_path->path) printf("target_path left\n");
    //  else printf("target_path right\n");

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

    printf("insert_node\n");
    printf("tracking_path\n");
    while(td->right){
      temp = td;
      td = td->right;
      free(temp);

      if(td->path){
          printf("target_path left\n");
          if(!(child_node->left)){
              child_node->left = new_node;
              new_node->parent = child_node;
          }
          child_node = child_node->left;
      }else{
          printf("target_path right\n");
          if(!(child_node->right)){
              child_node->right = new_node;
              new_node->parent = child_node;
          }
          child_node = child_node->right;
      }
      printf("the data: %d\n", child_node->data);
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

  printf("refresh_latest_node\n");
  printf("tracking_path\n");
  while(td->right){
    temp = td;
    td = td->right;
    free(temp);

    if(td->path){
        printf("target_path left\n");
        if(!(child_node->left)){
             latest_node = child_node->left;
             printf("  data: %d\n", child_node->left->data);
        }
        child_node = child_node->left;
    }else{
        printf("target_path right\n");
        if(!(child_node->right)){
            latest_node = child_node->right;
            printf("  data: %d\n", child_node->right->data);

        }
        child_node = child_node->right;
    }
  }
}


struct node* root_entry(struct node *root){
  struct node *temp;
  printf("root %d latest_node %d\n", root->data, latest_node->data);
  swap(root, latest_node);
  printf("root %d latest_node %d\n", root->data, latest_node->data);
  heapify_downward(root);
  total_nodes--;
  return latest_node;
}



int main(){

  struct node *root;
  struct node *temp;
  total_nodes = -1;
  root = get_new_node(6);

  // 1 2 3 4 5 6
  int i;
  for(i=5; i>=1; i--){
    // printf("insert_node begin\n");
    insert_node(root, i);
  }

  refresh_latest_node(root);
  printf("data remove ------- BEGIN\n");
  temp = root_entry(root);
  printf("data taken out: %d\n", temp->data);
  refresh_latest_node(root);

  printf("root: %d\n", root->data);

  temp = root_entry(root);
  printf("data taken out: %d\n", temp->data);
  refresh_latest_node(root);

  temp = root_entry(root);
  printf("data taken out: %d\n", temp->data);
  refresh_latest_node(root);


  printf("data remove ------- END\n");

  return 0;
}

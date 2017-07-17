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


// keep track of total number of nodes
static int total_nodes;

struct node{
  int data;
  struct node *left;
  struct node *right;
};

struct track_node{
  int path;
  struct track_node *right;
};



struct node* get_new_node(int data){
  struct node *new_node = (struct node *) malloc(sizeof(struct node));
  new_node->data = data;
  new_node->left = new_node->right = NULL;

  total_nodes = 0; // increase number of nodes
  return new_node;
}




// BEGIN struct track_location(total_number)

    // track_number = total_nubmer

    // if there is no previous truct
       // malloc struct with left and right pointer
       // add the right

    // while( track_number is not zero )
       // track_number = total_number

       // if the track_number is odd = left = 0
       // if the track_number is even = right = 1


       // if the current_track is not zero
          // malloc struct and connect it with the right pointer

       // track_number = divided_number
    // end while()


    // return the final struct

// END struct track_location



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


void insert_node(struct node *root, struct node *child){
    struct track_node *td;

    total_nodes++; //1
    td = tracking_nodes(total_nodes);

    printf("tracking_path\n");
    while(td->right){
      td = td->right;

      if(td->path) printf("target_path left\n");
      else printf("target_path right\n");
    }



}






//
// void swap(node *n1, node *n2) {
//     node temp = *n1 ;
//     *n1 = *n2 ;
//     *n2 = temp ;
// }
//




int main(){

  struct node *root;
  struct node *child;
  root = get_new_node(5);


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 32;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  printf("---------------------------------------------\n");
  child = (struct node *) malloc(sizeof(struct node));
  child->data = 33;
  insert_node(root, child);
  printf("---------------------------------------------\n");


  return 0;
}





/*
    Function to swap data within two nodes of the min heap using pointers
*/





// BEGIN struct track_location(total_number)

    // track_number = total_nubmer

    // if there is no previous truct
       // malloc struct with left and right pointer
       // add the right

    // while( track_number is not zero )
       // track_number = total_number

       // if the track_number is odd = left = 0
       // if the track_number is even = right = 1


       // if the current_track is not zero
          // malloc struct and connect it with the right pointer

       // track_number = divided_number
    // end while()


    // return the final struct

// END struct track_location



// BEGIN node move_to_final_destination(current_struct, new_node)

  // final_struct

  // while(current_struct->right is not NULL)
    // check the struct's data
       // if it is 0 move to left
       // if it is 1 move to right

       // final_struct = current_struct
  // end while

  // if ( final_struct->data is zero )
        // add the new node to left

  // else
        // add the new node to right

// END node move_to_final_destination




// BEGIN void heapify(root, selected_node)

  // if left child is the smallest?
  // else if right child is the smallest ?

  // selected_node = selected child

  // if selected child is not the root,
    // swap
    // heapify (selected child)
// END void heapify











// -hints
// void heapify(minHeap *hp, int i) {
//     int smallest = (LCHILD(i) < hp->size && hp->elem_test[LCHILD(i)]->data < hp->elem_test[i]->data) ? LCHILD(i) : i ;
//
//     // printf("compared: %d  %d  smallest : %d\n", hp->elem_test[LCHILD(i)]->data, hp->elem_test[i]->data, smallest);
//
//     if(RCHILD(i) < hp->size && hp->elem_test[RCHILD(i)]->data < hp->elem_test[smallest]->data) {
//         smallest = RCHILD(i) ;
//     }
//
//     if(smallest != i) {
//         swap(hp->elem_test[i], hp->elem_test[smallest]) ;
//         heapify(hp, smallest) ;
//     }
// }
//
//
//
// void maxHeapify(int A[],int n, int i)
//    {
//        int largest = i;  // Initialize largest as root
//        int l = 2*i + 1;  // left = 2*i + 1
//        int r = 2*i + 2;  // right = 2*i + 2
//
//        // If left child is larger than root
//        if (l < n && A[l] > A[largest])
//            largest = l;
//
//        // If right child is larger than largest so far
//        if (r < n && A[r] > A[largest])
//            largest = r;
//
//        // If largest is not root
//        if (largest != i)
//        {
//
//            swap(i,largest);
//            // Recursively heapify the affected sub-tree
//            maxHeapify(A,n,largest);
//        }
//    }

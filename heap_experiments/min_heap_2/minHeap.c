#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "minHeap.h"


/*
    Function to initialize the min heap with size = 0
*/
minHeap initMinHeap() {
    minHeap hp ;
    hp.size = 0 ;
    return hp ;
}


/*
    Function to swap data within two nodes of the min heap using pointers
*/
void swap(node *n1, node *n2) {
    node temp = *n1 ;
    *n1 = *n2 ;
    *n2 = temp ;
}


/*
    Heapify function is used to make sure that the heap property is never violated
    In case of deletion of a node, or creating a min heap from an array, heap property
    may be violated. In such cases, heapify function can be called to make sure that
    heap property is never violated
*/
void heapify(minHeap *hp, int i) {
    int smallest = (LCHILD(i) < hp->size && hp->elem_test[LCHILD(i)]->data < hp->elem_test[i]->data) ? LCHILD(i) : i ;

    // printf("compared: %d  %d  smallest : %d\n", hp->elem_test[LCHILD(i)]->data, hp->elem_test[i]->data, smallest);

    if(RCHILD(i) < hp->size && hp->elem_test[RCHILD(i)]->data < hp->elem_test[smallest]->data) {
        smallest = RCHILD(i) ;
    }

    if(smallest != i) {
        swap(hp->elem_test[i], hp->elem_test[smallest]) ;
        heapify(hp, smallest) ;
    }
}


/*
    Build a Min Heap given an array of numbers
    Instead of using insertNode() function n times for total complexity of O(nlogn),
    we can use the buildMinHeap() function to build the heap in O(n) time
*/
void buildMinHeap(minHeap *hp, int *arr, int size) {
    int i ;

    // Insertion into the heap without violating the shape property
    for(i = 0; i < size; i++) {
        if(hp->size) {
            hp->elem = (node *)realloc(hp->elem, (hp->size + 1) * sizeof(node)) ;
        } else {
            hp->elem = (node *)malloc(sizeof(node)) ;
        }
        node nd ;
        nd.data = arr[i] ;
        hp->elem[(hp->size)++] = nd ;
    }

    // Making sure that heap property is also satisfied
    for(i = (hp->size - 1) / 2; i >= 0; i--) {
        heapify(hp, i) ;
    }
}


/*
    Function to insert a node into the min heap, by allocating space for that node in the
    heap and also making sure that the heap property and shape propety are never violated.
*/
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

/*
void insertNode(minHeap *hp, int data) {
    if(hp->size) {
        hp->elem = (node *)realloc(hp->elem, (hp->size + 1) * sizeof(node)) ;
    } else {
        hp->elem = (node *)malloc(sizeof(node)) ;
    }

    node nd ;
    nd.data = data ;

    int i = (hp->size)++ ;
    while(i && nd.data < hp->elem[PARENT(i)].data) {
        hp->elem[i] = hp->elem[PARENT(i)] ;
        i = PARENT(i) ;
    }
    hp->elem[i] = nd ;
}
*/

/*
    Function to delete a node from the min heap
    It shall remove the root node, and place the last node in its place
    and then call heapify function to make sure that the heap property
    is never violated
*/
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

// void deleteNode(minHeap *hp) {
//     if(hp->size) {
//         hp->elem[0] = hp->elem[--(hp->size)] ;
//         hp->elem = (node *)realloc(hp->elem, hp->size * sizeof(node)) ;
//         heapify(hp, 0) ;
//     } else {
//         free(hp->elem) ;
//     }
// }



/*
    Function to get maximum node from a min heap
    The maximum node shall always be one of the leaf nodes. So we shall recursively
    move through both left and right child, until we find their maximum nodes, and
    compare which is larger. It shall be done recursively until we get the maximum
    node
*/
int getMaxNode(minHeap *hp, int i) {
    if(LCHILD(i) >= hp->size) {
        return hp->elem[i].data ;
    }

    int l = getMaxNode(hp, LCHILD(i)) ;
    int r = getMaxNode(hp, RCHILD(i)) ;

    if(l >= r) {
        return l ;
    } else {
        return r ;
    }
}


/*
    Function to clear the memory allocated for the min heap
*/
void deleteMinHeap(minHeap *hp) {
    free(hp->elem) ;
}


/*
    Function to display all the nodes in the min heap by doing a inorder traversal
*/
void inorderTraversal(minHeap *hp, int i) {
    if(LCHILD(i) < hp->size) {
        inorderTraversal(hp, LCHILD(i)) ;
    }
    printf("%d ", hp->elem[i].data) ;
    if(RCHILD(i) < hp->size) {
        inorderTraversal(hp, RCHILD(i)) ;
    }
}


/*
    Function to display all the nodes in the min heap by doing a preorder traversal
*/
void preorderTraversal(minHeap *hp, int i) {
    if(LCHILD(i) < hp->size) {
        preorderTraversal(hp, LCHILD(i)) ;
    }
    if(RCHILD(i) < hp->size) {
        preorderTraversal(hp, RCHILD(i)) ;
    }
    printf("%d ", hp->elem[i].data) ;
}


/*
    Function to display all the nodes in the min heap by doing a post order traversal
*/
void postorderTraversal(minHeap *hp, int i) {
    printf("%d ", hp->elem[i].data) ;
    if(LCHILD(i) < hp->size) {
        postorderTraversal(hp, LCHILD(i)) ;
    }
    if(RCHILD(i) < hp->size) {
        postorderTraversal(hp, RCHILD(i)) ;
    }
}


/*
    Function to display all the nodes in the min heap by doing a level order traversal
*/
void levelorderTraversal(minHeap *hp) {
    int i ;
    for(i = 0; i < hp->size; i++) {
        printf("%d ", hp->elem[i].data) ;
    }
}

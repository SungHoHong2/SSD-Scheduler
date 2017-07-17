#include <stdio.h>
#include <stdlib.h>

struct BstNode{
  int data;
  struct BstNode *left;
  struct BstNode *right;
};


struct BstNode* GetNewNode(int data);
struct BstNode* Insert(struct BstNode *root, int data);
struct BstNode* Search(struct BstNode *root, int data);
struct BstNode* Delete(struct BstNode *root);


int main(){
   struct BstNode *root = NULL;
   struct BstNode *node;

   root = Insert(root, 15);
   root = Insert(root, 10);
   root = Insert(root, 20);

   node = Search(root, 15);
   if(node) printf("search: %d\n", node->data);
   else printf("no node\n");

   node = Search(root, 30);
   if(node) printf("search: %d\n", node->data);
   else printf("no node\n");

   node = Delete(root);
   if(node) printf("delete: %d\n", node->data);
   else printf("no node\n");


   node = Delete(root);
   if(node) printf("delete: %d\n", node->data);
   else printf("no node\n");


   node = Delete(root);
   if(node) printf("delete: %d\n", node->data);
   else printf("no node\n");


   node = Delete(root);
   if(node) printf("delete: %d\n", node->data);
   else printf("no node\n");


  return 0;

}


struct BstNode* GetNewNode(int data){
  struct BstNode *newNode = (struct BstNode *) malloc(sizeof(struct BstNode));
  newNode->data = data;
  newNode->left = newNode->right = NULL;
  return newNode;
}



struct BstNode* Insert(struct BstNode *root, int data){

  if(root == NULL){
      root = GetNewNode(data);
  } else if (data <= root->data){
      root->left = Insert(root->left, data);
  } else {
      root->right = Insert(root->left, data);
  }

  return root;
}

struct BstNode* Search(struct BstNode *root, int data){
  if(root == NULL) return NULL;
  else if(root->data == data) return root;
  else if(data <= root->data) return Search(root->left,data);
  else return Search(root->right, data);
}


struct BstNode* Delete(struct BstNode *root){
  if(root == NULL) return NULL;
  else{

    // no child
    if(root->left == NULL && root->right == NULL){
        free(root);

    // geting a child from right
    } else if (root->left == NULL){
        struct BstNode *temp = root;
        root = root->right;
        free(temp);

    // geting a child from left
    } else if (root->right == NULL){
        struct BstNode *temp = root;
        root = root->left;
        free(temp);

    } else{

      struct BstNode *temp = FindMin(root->right);
      root->data = temp->data;
      root->right = Delete(root->right);



    }

    return root;
  }
}

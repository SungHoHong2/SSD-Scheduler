#include <linux/module.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/rbtree.h>

struct unode{
       struct rb_node __rb_node;
       int __num;      //this is to record our data.
};


struct unode * rb_insert_unode( struct rb_root * root , int target , struct rb_node * source ){

       struct rb_node **p = &root->rb_node;
       struct rb_node *parent = NULL;
       struct unode * ans;

       while( *p ){

              parent = *p;
              ans = rb_entry( parent , struct unode , __rb_node );

              if( target < ans->__num )
                     p = &(*p)->rb_left;
              else if( target > ans->__num )
                     p = &(*p)->rb_right;
              else
                     return ans;

       }
       rb_link_node( source , parent , p );  //Insert this new node as a red leaf.
       rb_insert_color( source , root );  //Rebalance the tree, finish inserting
       return NULL;
}


struct unode * rb_search_unode( struct rb_root * root , int target ){

       struct rb_node * n = root->rb_node;
       struct unode * ans;

       while( n ){
              //Get the parent struct to obtain the data for comparison
              ans = rb_entry( n , struct unode , __rb_node );

              if( target < ans->__num ){
                     n = n->rb_left;
              }else if( target > ans->__num ){
                     n = n->rb_right;
              }else
                     return ans;

       }
       return NULL;

}



void rb_erase_unode( struct rb_node * source , struct rb_root * root ){
       struct unode * target;
       target = rb_entry( source , struct unode , __rb_node );
       rb_erase( source , root );
       kfree( target );

}




static int __init rbtree_init(void){

	struct rb_root root = RB_ROOT;
  struct unode *node;

	printk("RB-TREE INSERT\n");

	node = ( struct unode * )kmalloc( sizeof(struct unode), GFP_KERNEL);
	node->__num = 1;
	rb_insert_unode( &root , 1 , &node->__rb_node );

	node = ( struct unode * )kmalloc( sizeof(struct unode), GFP_KERNEL);
	node->__num = 2;

	rb_insert_unode( &root , 2 , &node->__rb_node );

	node = ( struct unode * )kmalloc( sizeof(struct unode), GFP_KERNEL);
	node->__num = 3;

  rb_insert_unode( &root , 3 , &node->__rb_node );

	printk("RB-TREE INSERT END\n");


	node = rb_search_unode( &root , 1);
	if(node) printk("RB-TREE SEARCH %d\n", node->__num);

	node = rb_search_unode( &root , 2);
 	if(node) printk("RB-TREE SEARCH %d\n", node->__num);

	node = rb_search_unode( &root , 3);
	if(node) printk("RB-TREE SEARCH %d\n", node->__num);


	printk("RB-TREE ERASE\n");
	rb_erase_unode( &node->__rb_node , &root );


	node = rb_search_unode( &root , 3);
	if(node) printk("RB-TREE SEARCH %d\n", node->__num);
	else printk("DOESN't EXISTS\n");


	return 0;
}

static void __exit rbtree_exit(void){


}

module_init(rbtree_init);
module_exit(rbtree_exit);


MODULE_AUTHOR("Sungho Hong");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RB-Tree test");

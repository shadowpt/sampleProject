#include "RBT.h"

#define NOCOLOR 0
#define RED 1
#define BLACK 2

struct node *create_nil();

/* Initialize the Red Black Tree header */
struct rbt *init_tree(){

	struct rbt *tree = malloc(sizeof(struct rbt));

	tree->root = NULL;
	tree->first = NULL;
	tree->nil = create_nil();

	return tree;

}

struct node *create_node(struct rbt *tree, int num){

	struct node *node = malloc(sizeof(struct node));

	node->color = RED;
	node->num = num;
	node->parent = tree->nil;	
	node->left = tree->nil;
	node->right = tree->nil;

	return node;

}

struct node *create_nil()
{
	struct node *nil = malloc(sizeof(struct node));
	nil->color = BLACK;
	nil->num = 0;
	nil->parent = NULL;
	nil->left = NULL;
	nil->right = NULL;

	return nil;
}

void lower(struct rbt * tree)
{

	struct node * node;

	for (node = tree->first; node->left != tree->nil; node = node->left);

	tree->first = node;

}

void printNode(struct rbt *tree, struct node *node)
{
	if (node != tree->nil){

		puts("------------------------");
		printf("Num = %d\n", node->num);

		if (node->color == RED)
			puts("RED");
		else puts("BLACK");
	
		if (isRoot(tree, node))
			puts("I'm Root");
		else printf("Parent = %d\n", node->parent->num);

		puts("Left");
		printNode(tree, node->left);

		puts("Right");
		printNode(tree, node->right);

	}
	else puts("NIL");

}

void printTree(struct rbt *tree)
{
	printNode(tree, tree->root);
	puts("--------------------");
}

void changeColor(struct rbt *tree, struct node *node, int color)
{

	if(node == tree->nil)
		return;
	else node->color = color;
}

int isRoot(struct rbt *tree, struct node *node)
{
	return node->parent == tree->nil;
}

struct node *grandparent(struct node *node)
{
	return node->parent->parent;
}

struct node *sibling_node(struct node *node)
{

	if(node->parent->right == node)
		return node->parent->left;

	else return node->parent->right;

}

int siblingColor(struct node *node)
{

	struct node *sibling = sibling_node(node);

	return sibling->color;

}

void treeRoot(struct rbt *tree){

	struct node *node = tree->root;
	
	while(node->parent != tree->nil)
		node = node->parent;

	tree->root = node;
}	

void rightRightCase(struct rbt *tree, struct node *node)
{
	struct node *tmp = grandparent(node)->parent;

	grandparent(node)->right = node->parent->left;
	if (grandparent(node)->right != tree->nil)
		grandparent(node)->right->parent = grandparent(node);
	node->parent->left = grandparent(node);
	grandparent(node)->parent = node->parent;
	node->parent->parent = tmp;

	if(!isRoot(tree, node->parent)){
		if(grandparent(node)->right == node->parent->left)
			grandparent(node)->right = node->parent;
		else grandparent(node)->left = node->parent;
	}

	changeColor(tree, node->parent, BLACK);
	changeColor(tree, node->parent->left, RED);
	changeColor(tree, node->parent->right, RED);

}

void leftLeftCase(struct rbt *tree, struct node *node)
{
	struct node *tmp = grandparent(node)->parent;

	grandparent(node)->left = node->parent->right;
	if (grandparent(node)->left != tree->nil)
		grandparent(node)->left->parent = grandparent(node);
	node->parent->right = grandparent(node);
	grandparent(node)->parent = node->parent;
	node->parent->parent = tmp;

	if(!isRoot(tree, node->parent)){
		if(grandparent(node)->right == node->parent->right)
			grandparent(node)->right = node->parent;
		else grandparent(node)->left = node->parent;
	}

	changeColor(tree, node->parent, BLACK);
	changeColor(tree, node->parent->left, RED);
	changeColor(tree, node->parent->right, RED);

}

void leftRightCase(struct rbt *tree, struct node *node)
{
	struct node *tmp = node->parent;

	node->parent = grandparent(node);
	tmp->right = node->left;
	if(tmp->right != tree->nil)
		tmp->right->parent = tmp;
	node->left = tmp;
	tmp->parent = node;
	node->parent->left = node;
	leftLeftCase(tree, node->left);

}

void rightLeftCase(struct rbt *tree, struct node *node)
{
	struct node *tmp = node->parent;

	node->parent = grandparent(node);
	tmp->left = node->right;
	if (tmp->left != tree->nil)
		tmp->left->parent = tmp;
	node->right = tmp;
	tmp->parent = node;
	node->parent->right = node;
	rightRightCase(tree, node->right);

}

void reconstruct(struct rbt *tree, struct node *node)
{

	if(grandparent(node)->left != tree->nil){
		if (grandparent(node)->left->left == node)
			leftLeftCase(tree, node);

		else if (grandparent(node)->left->right == node)
			leftRightCase(tree, node);
	}
	else if(grandparent(node)->right != tree->nil){
		if(grandparent(node)->right->right == node)
			rightRightCase(tree, node);

		else rightLeftCase(tree, node);
	}

}

void correctDoubleRed(struct rbt *tree, struct node *node)
{
	if(isRoot(tree, node->parent))
		return;
	if(node->parent->color != RED)
		return;
	if(siblingColor(node->parent) != RED){
		reconstruct(tree, node);
	}

	else {
		changeColor(tree, node->parent, BLACK);
			changeColor(tree, sibling_node(node->parent), BLACK);
		if(isRoot(tree, grandparent(node)))
			return;
		changeColor(tree, grandparent(node), RED);
		correctDoubleRed(tree, grandparent(node));
	}
}

void insert_node_right(struct rbt *tree, struct node *current_node, struct node *parent, struct node *add_item);

void insert_node_left(struct rbt *tree, struct node *current_node, struct node *parent, struct node *add_item)
{
	if (current_node == tree->nil){
		add_item->parent = parent;
		parent->left = add_item;
	}
	else if (add_item->num > current_node->num)
		insert_node_right(tree, current_node->right, current_node, add_item);
	else insert_node_left(tree, current_node->left, current_node, add_item);

}

void insert_node_right(struct rbt *tree, struct node *current_node, struct node *parent, struct node *add_item)
{
	if (current_node == tree->nil){
		add_item->parent = parent;
		parent->right = add_item;
	}
	else if (add_item->num > current_node->num)
		insert_node_right(tree, current_node->right, current_node, add_item);
	else insert_node_left(tree, current_node->left, current_node, add_item);

}


void insert_node(struct rbt* tree, int num){

	struct node *current_node = tree->root;
	struct node *next_node;
	struct node *add_item = create_node(tree, num);

	if(current_node == NULL) {
		add_item->color = BLACK;
		tree->root = add_item;
		tree->first = tree->root;
		return;
	}

	if(num < current_node->num)
		insert_node_left(tree, current_node->left, current_node, add_item);
	else insert_node_right(tree, current_node->right, current_node, add_item);

	correctDoubleRed(tree, add_item);
	treeRoot(tree);
	lower(tree);

}

struct node *find_node(struct rbt *tree, int num)
{
	struct node *current_node;

	for(current_node = tree->root; current_node != tree->nil; ){
		if(current_node->num == num)
			return current_node;
		
		if(current_node->num < num)
			current_node = current_node->right;
		else current_node = current_node->left;
	}

	return NULL;
}

struct node *minimum(struct rbt *tree, struct node *node)
{
	while (node->left != tree->nil)
		node = node->left;

	return node;
}

struct node *maximum(struct rbt *tree, struct node *node)
{
	while (node->right != tree->nil)
		node = node->right;

	return node;
}

struct node *sucessor(struct rbt *tree, struct node *node)
{
	struct node *parent;
	
	if(node->right != tree->nil)
		return minimum(tree, node->right);

	parent = node->parent;
	while(parent != tree->nil && node == parent->right){
		node = parent;
		parent = parent->parent;
	}
	return node;
}

struct node *predecessor(struct rbt *tree, struct node *node)
{
	if(node->left == tree->nil)
		return NULL;

	return maximum(tree, node->left);
}

int black_leef(struct rbt *tree, struct node* node)
{
	if(node->color == BLACK && node->right == tree->nil && node->left == tree->nil)
		return 1;

	return 0;
}

void substitute(struct rbt *tree, struct node *x, struct node *y)
{
	y->parent = x->parent;
	y->right = x->right;

	if(y->right !=tree->nil)
		y->right->parent = y;

	y->left = x->left;

	if(y->left != tree->nil)
		y->left->parent = y;

	y->color = x->color;

	if(y->parent != tree->nil){
		if (y->parent->left == x)
			y->parent->left = y;
		else y->parent->right = y;
	}
}

void left_rotate(struct rbt *tree, struct node *node)
{
	struct node *y = node->right;

	node->right = y->left;
	y->left->parent = node;
	y->parent = node->parent;

	if(node->parent == tree->nil)
		tree->root = y;
	else if (node == node->parent->left)
		node->parent->left = y;
	else node->parent->right = y;

	y->left = node;
	node->parent = y;
}

void right_rotate(struct rbt *tree, struct node *node)
{
	struct node *y = node->left;

	node->left = y->right;
	y->right->parent = node;
	y->parent = node->parent;

	if (node->parent == tree->nil)
		tree->root = y;
	else if (node == node->parent->right)
		node->parent->right = y;
	else node->parent->left = y;

	y->right = node;
	node->parent = y;
}

void right_case_1(struct rbt *tree, struct node *node, struct node *sibling)
{
	
} 

void left_case_1(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void case_2(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void right_case_3(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void left_case_3(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void right_case_4(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void left_case_4(struct rbt *tree, struct node *node, struct node *sibling)
{
	
}

void delete_fixup(struct rbt *tree, struct node *node)
{
	struct node *sibling;

	while(!isRoot(tree, node) && node->color == BLACK){
		if (node->parent->left == node){
			sibling = node->parent->right;

			if(sibling->color == RED){
				sibling->color = BLACK;
				node->parent->color = RED;
				left_rotate(tree, node->parent);
				sibling = node->parent->right;
			}

			if(sibling->left->color == BLACK && sibling->right->color == BLACK){
				sibling->color = RED;
				node = node->parent;
			}

			else {
				if (sibling->right->color == BLACK){
					sibling->left->color = BLACK;
					sibling->color = RED;
					right_rotate(tree, sibling);
					sibling = node->parent->right;
				}

				sibling->color = node->parent->color;
				node->parent->color = BLACK;
				sibling->right->color = BLACK;
				left_rotate(tree, node->parent);
				node = tree->root;
			}
		}
		
		else {
			sibling = node->parent->left;

			if(sibling->color == RED){
				sibling->color = BLACK;
				node->parent->color = RED;
				right_rotate(tree, node->parent);
				sibling = node->parent->left;
			}

			if(sibling->left->color == BLACK && sibling->right->color == BLACK){
				sibling->color = RED;
				node = node->parent;
			}

			else {
				if (sibling->left->color == BLACK){
					sibling->right->color = BLACK;
					sibling->color = RED;
					left_rotate(tree, sibling);
					sibling = node->parent->left;
				}

				sibling->color = node->parent->color;
				node->parent->color = BLACK;
				sibling->left->color = BLACK;
				right_rotate(tree, node->parent);
				node = tree->root;
			}
		}
	}
	node->color = BLACK;
}

struct node *remove_node(struct rbt *tree, int num)
{
	struct node *delete_node = find_node(tree, num);
	struct node *y;
	struct node *x;

	if(delete_node == NULL){
		printf("Node with num = %d doesn't exist\n", num);
		return;
	}

	if(delete_node->left == tree->nil || delete_node->right == tree->nil)
		y = delete_node;
	else {	
		y = sucessor(tree, delete_node);	
		if(!(y->color == RED || !black_leef(tree, y)))
			y = predecessor(tree, delete_node);
	}

	if (y->left != tree->nil)
		x = y->left;
	else x = y->right;
	
	x->parent = y->parent;

	if (y->parent == tree->nil)
		tree->root = x;
	else { 
		if (y == y->parent->left)
			y->parent->left = x;
		else y->parent->right = x;
	}

	if(y != delete_node){
		substitute(tree, delete_node, y);
		if (isRoot(tree, y))
			tree->root = y;
	}

	if(y->color == BLACK)
		delete_fixup(tree, x);

	treeRoot(tree);

	return y;
}

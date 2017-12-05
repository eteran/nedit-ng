
/*
** rbTree is a red-black balanced binary tree
** the base node holds the leftmost, rightmost and root pointers
** and a node count
*/

#include "rbTree.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int rbTreeNodeRed   = 0;
constexpr int rbTreeNodeBlack = 1;

/*
** rotate a node left
*/
void rotateLeft(rbTreeNode *x, rbTreeNode **root) {
	rbTreeNode *y = x->right;
	x->right = y->left;
	if (y->left) {
		y->left->parent = x;
	}
	y->parent = x->parent;

	if (x == *root) {
		*root = y;
	} else if (x == x->parent->left) {
		x->parent->left = y;
	} else {
		x->parent->right = y;
	}
	y->left = x;
	x->parent = y;
}

/*
** rotate a node right
*/
void rotateRight(rbTreeNode *x, rbTreeNode **root) {
	rbTreeNode *y = x->left;
	x->left = y->right;
	if (y->right) {
		y->right->parent = x;
	}
	y->parent = x->parent;

	if (x == *root) {
		*root = y;
	} else if (x == x->parent->right) {
		x->parent->right = y;
	} else {
		x->parent->left = y;
	}
	y->right = x;
	x->parent = y;
}

/*
** balance tree after an insert of node x
*/
void insertBalance(rbTreeNode *x, rbTreeNode **root) {
	x->color = rbTreeNodeRed;
	while (x != *root && x->parent->color == rbTreeNodeRed) {
		if (x->parent == x->parent->parent->left) {
			rbTreeNode *y = x->parent->parent->right;
			if (y && y->color == rbTreeNodeRed) {
				x->parent->color = rbTreeNodeBlack;
				y->color = rbTreeNodeBlack;
				x->parent->parent->color = rbTreeNodeRed;
				x = x->parent->parent;
			} else {
				if (x == x->parent->right) {
					x = x->parent;
					rotateLeft(x, root);
				}
				x->parent->color = rbTreeNodeBlack;
				x->parent->parent->color = rbTreeNodeRed;
				rotateRight(x->parent->parent, root);
			}
		} else {
			rbTreeNode *y = x->parent->parent->left;
			if (y && y->color == rbTreeNodeRed) {
				x->parent->color = rbTreeNodeBlack;
				y->color = rbTreeNodeBlack;
				x->parent->parent->color = rbTreeNodeRed;
				x = x->parent->parent;
			} else {
				if (x == x->parent->left) {
					x = x->parent;
					rotateRight(x, root);
				}
				x->parent->color = rbTreeNodeBlack;
				x->parent->parent->color = rbTreeNodeRed;
				rotateLeft(x->parent->parent, root);
			}
		}
	}
	(*root)->color = rbTreeNodeBlack;
}

}

/*
** returns the leftmost node (the beginning of the sorted list)
*/
rbTreeNode *rbTreeBegin(rbTreeNode *base) {
	return (base->left);
}

/*
** returns the rightmost node (the end of the sorted list)
*/
rbTreeNode *rbTreeReverseBegin(rbTreeNode *base) {
	return (base->right);
}

/*
** search for a node and return it's pointer, nullptr if not found
*/
rbTreeNode *rbTreeFind(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords) {
	rbTreeNode *foundNode = nullptr;
	rbTreeNode *current = base->parent;
	while (current) {
		int compareResult = compareRecords(searchNode, current);

		if (compareResult < 0) {
			current = current->left;
		} else if (compareResult > 0) {
			current = current->right;
		} else {
			foundNode = current;
			current = nullptr;
		}
	}
	return (foundNode);
}

/*
** insert a node into the tree and rebalance it
** if a duplicate is found copy the new data over it
** returns the new node
*/
rbTreeNode *rbTreeInsert(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords, rbTreeAllocateNodeCB allocateNode, rbTreeCopyToNodeCB copyToNode) {
	rbTreeNode *current, *parent, *x;
	int fromLeft = 0, foundMatch = 0;

	current = base->parent;
	parent = nullptr;
	x = nullptr;
	while (current) {
		int compareResult = compareRecords(searchNode, current);

		if (compareResult < 0) {
			parent = current;
			current = current->left;
			fromLeft = 1;
		} else if (compareResult > 0) {
			parent = current;
			current = current->right;
			fromLeft = 0;
		} else {
			x = current;
			if (!copyToNode(x, searchNode)) {
				x = nullptr;
			}
			current = nullptr;
			foundMatch = 1;
		}
	}

	if (!foundMatch) {
		x = allocateNode(searchNode);
		if (x) {
			x->parent = parent;
			x->left = nullptr;
			x->right = nullptr;
			x->color = rbTreeNodeRed;

			if (parent) {
				if (fromLeft) {
					parent->left = x;
				} else {
					parent->right = x;
				}
			} else {
				base->parent = x;
			}
			++(base->color);
			if (x->parent == base->left && (x->parent == nullptr || x->parent->left == x)) {
				base->left = x;
			}
			if (x->parent == base->right && (x->parent == nullptr || x->parent->right == x)) {
				base->right = x;
			}
			insertBalance(x, &base->parent);
		}
	}
	return (x);
}

/*
** unlink a node from the tree and rebalance it.
** returns the removed node pointer
*/
rbTreeNode *rbTreeUnlinkNode(rbTreeNode *base, rbTreeNode *z) {
	rbTreeNode *x;
	rbTreeNode *x_parent;

	rbTreeNode *y = z;
	if (y->left == nullptr) {
		x = y->right;
		if (y == base->left) {
			base->left = rbTreeNext(y);
		}
	} else {
		if (y->right == nullptr) {
			x = y->left;
			if (y == base->right) {
				base->right = rbTreePrevious(y);
			}
		} else {
			y = y->right;
			while (y->left) {
				y = y->left;
			}
			x = y->right;
		}
	}
	if (y != z) {
		z->left->parent = y;
		y->left = z->left;
		if (y != z->right) {
			x_parent = y->parent;
			if (x) {
				x->parent = y->parent;
			}
			y->parent->left = x;
			y->right = z->right;
			z->right->parent = y;
		} else {
			x_parent = y;
		}
		if (base->parent == z) {
			base->parent = y;
		} else if (z->parent->left == z) {
			z->parent->left = y;
		} else {
			z->parent->right = y;
		}
		y->parent = z->parent;

		std::swap(y->color, z->color);

		y = z;
	} else {
		x_parent = y->parent;
		if (x) {
			x->parent = y->parent;
		}
		if (base->parent == z) {
			base->parent = x;
		} else {
			if (z->parent->left == z) {
				z->parent->left = x;
			} else {
				z->parent->right = x;
			}
		}
	}

	--(base->color);

	if (y->color != rbTreeNodeRed) {
		while (x != base->parent && (x == nullptr || x->color == rbTreeNodeBlack)) {
			if (x == x_parent->left) {
				rbTreeNode *w = x_parent->right;
				if (w->color == rbTreeNodeRed) {
					w->color = rbTreeNodeBlack;
					x_parent->color = rbTreeNodeRed;
					rotateLeft(x_parent, &base->parent);
					w = x_parent->right;
				}
				if ((w->left == nullptr || w->left->color == rbTreeNodeBlack) && (w->right == nullptr || w->right->color == rbTreeNodeBlack)) {

					w->color = rbTreeNodeRed;
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					if (w->right == nullptr || w->right->color == rbTreeNodeBlack) {

						if (w->left) {
							w->left->color = rbTreeNodeBlack;
						}
						w->color = rbTreeNodeRed;
						rotateRight(w, &base->parent);
						w = x_parent->right;
					}
					w->color = x_parent->color;
					x_parent->color = rbTreeNodeBlack;
					if (w->right) {
						w->right->color = rbTreeNodeBlack;
					}
					rotateLeft(x_parent, &base->parent);
					break;
				}
			} else {
				rbTreeNode *w = x_parent->left;
				if (w->color == rbTreeNodeRed) {
					w->color = rbTreeNodeBlack;
					x_parent->color = rbTreeNodeRed;
					rotateRight(x_parent, &base->parent);
					w = x_parent->left;
				}
				if ((w->right == nullptr || w->right->color == rbTreeNodeBlack) && (w->left == nullptr || w->left->color == rbTreeNodeBlack)) {

					w->color = rbTreeNodeRed;
					x = x_parent;
					x_parent = x_parent->parent;
				} else {
					if (w->left == nullptr || w->left->color == rbTreeNodeBlack) {

						if (w->right) {
							w->right->color = rbTreeNodeBlack;
						}
						w->color = rbTreeNodeRed;
						rotateLeft(w, &base->parent);
						w = x_parent->left;
					}
					w->color = x_parent->color;
					x_parent->color = rbTreeNodeBlack;
					if (w->left) {
						w->left->color = rbTreeNodeBlack;
					}
					rotateRight(x_parent, &base->parent);
					break;
				}
			}
		}
		if (x) {
			x->color = rbTreeNodeBlack;
		}
	}
	return (y);
}

/*
** delete an already found node and dispose it
*/
void rbTreeDeleteNode(rbTreeNode *base, rbTreeNode *foundNode, rbTreeDisposeNodeCB disposeNode) {
	disposeNode(rbTreeUnlinkNode(base, foundNode));
}

/*
** search for a node and remove it from the tree
** disposing the removed node
** returns 1 if a node was found, otherwise 0
*/
int rbTreeDelete(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords, rbTreeDisposeNodeCB disposeNode) {
	int foundNode = 0;
	rbTreeNode *z;

	z = rbTreeFind(base, searchNode, compareRecords);
	if (z) {
		rbTreeDeleteNode(base, z, disposeNode);
		foundNode = 1;
	}
	return (foundNode);
}

/*
** move an iterator foreward one element
** note that a valid pointer must be passed,
** passing nullptr will result in unpredictable results
*/
rbTreeNode *rbTreeNext(rbTreeNode *x) {
	if (x->right) {
		x = x->right;
		while (x->left) {
			x = x->left;
		}
	} else {
		rbTreeNode *fromX;
		do {
			fromX = x;
			x = x->parent;
		} while (x && fromX == x->right);
	}
	return (x);
}

/*
** move an iterator back one element
** note that a valid pointer must be passed,
** passing nullptr will result in unpredictable results
*/
rbTreeNode *rbTreePrevious(rbTreeNode *x) {
	if (x->left) {
		x = x->left;
		while (x->right) {
			x = x->right;
		}
	} else {
		rbTreeNode *fromX;
		do {
			fromX = x;
			x = x->parent;
		} while (x && fromX == x->left);
	}
	return (x);
}

/*
** returns the number of real data nodes in the tree, not counting
** the base node since it contains no data
*/
int rbTreeSize(rbTreeNode *base) {
	return base->color;
}

/*
** Allocate a new red-black tree using an empty node to hold pointers
*/
rbTreeNode *rbTreeNew(rbTreeAllocateEmptyNodeCB allocateEmptyNode) {
	rbTreeNode *rootStorage = allocateEmptyNode();
	if (rootStorage) {
		rootStorage->left   = nullptr;   // leftmost node 
		rootStorage->right  = nullptr;  // rightmost node 
		rootStorage->parent = nullptr; // root node 
		rootStorage->color  = 0;        // node count 
	}
	return rootStorage;
}


/*
** iterate through all nodes, unlinking and disposing them
** extra effort is made to maintain all links, the size, and
** leftmost/rightmost pointers, so that the tree can be dumped
** when debugging problems. We could probably ifdef some of this
** since it goes unused most of the time
** the tree is not kept balanced since all nodes will be removed
*/
void rbTreeDispose(rbTreeNode *base, rbTreeDisposeNodeCB disposeNode) {
	rbTreeNode *iter = rbTreeBegin(base);
	while (iter) {
		rbTreeNode *nextIter = rbTreeNext(iter);

		if (iter->parent) {
			if (iter->parent->left == iter) {
				iter->parent->left = iter->right;
			} else {
				iter->parent->right = iter->right;
			}
		}
		if (iter->right) {
			iter->right->parent = iter->parent;
		}
		base->left = nextIter;
		if (base->right == iter) {
			base->right = nullptr;
		}
		--(base->color);
		if (base->parent == iter) {
			base->parent = nextIter;
		}
		disposeNode(iter);

		iter = nextIter;
	}
	disposeNode(base);
}

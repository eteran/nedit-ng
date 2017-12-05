
#ifndef RBTREE_H_
#define RBTREE_H_

struct rbTreeNode {
    virtual ~rbTreeNode() = default;
	
	struct rbTreeNode *left;   /* left child */
	struct rbTreeNode *right;  /* right child */
	struct rbTreeNode *parent; /* parent */
	int color;                 /* node color (rbTreeNodeBlack, rbTreeNodeRed) */
};

using rbTreeCompareNodeCB       = int (*)(rbTreeNode *left, rbTreeNode *right);
using rbTreeAllocateNodeCB      = rbTreeNode *(*)(rbTreeNode *src);
using rbTreeAllocateEmptyNodeCB = rbTreeNode *(*)();
using rbTreeDisposeNodeCB       = void (*)(rbTreeNode *src);
using rbTreeCopyToNodeCB        = int (*)(rbTreeNode *dst, rbTreeNode *src);

rbTreeNode *rbTreeBegin(rbTreeNode *base);
rbTreeNode *rbTreeReverseBegin(rbTreeNode *base);
rbTreeNode *rbTreeFind(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords);
rbTreeNode *rbTreeInsert(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords, rbTreeAllocateNodeCB allocateNode, rbTreeCopyToNodeCB copyToNode);
rbTreeNode *rbTreeUnlinkNode(rbTreeNode *base, rbTreeNode *z);
void rbTreeDeleteNode(rbTreeNode *base, rbTreeNode *foundNode, rbTreeDisposeNodeCB disposeNode);
int rbTreeDelete(rbTreeNode *base, rbTreeNode *searchNode, rbTreeCompareNodeCB compareRecords, rbTreeDisposeNodeCB disposeNode);
rbTreeNode *rbTreeNext(rbTreeNode *x);
rbTreeNode *rbTreePrevious(rbTreeNode *x);
int rbTreeSize(rbTreeNode *base);
rbTreeNode *rbTreeNew(rbTreeAllocateEmptyNodeCB allocateEmptyNode);
void rbTreeDispose(rbTreeNode *base, rbTreeDisposeNodeCB disposeNode);

#endif

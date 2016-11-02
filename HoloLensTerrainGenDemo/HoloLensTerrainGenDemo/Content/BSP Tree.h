/*	Binary Space Partitioning Tree
	Stores start and end points for a line segment dividing space in two.
	Contains a pointer to the child nodes to the right and left of the dividing line.
	Contains a pointer to its own parent.
*/
#pragma once
#include <DirectXMath.h>

class BSPNode {
public:
	BSPNode();					
	// Constructor to create new Node with supplied parent node.
	BSPNode(BSPNode *parent);	
	~BSPNode();					
	void SetStartPos(float x, float y) { m_posStart.x = x; m_posStart.y = y; }
	void SetStartPos(DirectX::XMFLOAT2 p) { m_posStart = p; }
	void SetEndPos(float x, float y) { m_posEnd.x = x; m_posEnd.y = y; }
	void SetEndPos(DirectX::XMFLOAT2 p) { m_posEnd = p; }
	DirectX::XMFLOAT2 GetStartPos() { return m_posStart; }
	DirectX::XMFLOAT2 GetEndPos() {	return m_posEnd; }
	BSPNode *GetLeftChild() { return m_childLeft; }
	BSPNode *GetRightChild() { return m_childRight; }
	BSPNode *GetParent() { return m_parent; }
	// Create the left child and return a pointer to it;
	BSPNode *CreateLeftChild();		
	// Create the right child and return a pointer to it;
	BSPNode *CreateRightChild();	

private:
	BSPNode*			m_childLeft;	// Left child node.
	BSPNode*			m_childRight;	// Right child node.
	BSPNode*			m_parent;		// The parent of this node.
	DirectX::XMFLOAT2	m_posStart;		// Position of starting point of line segment representing line of the node.
	DirectX::XMFLOAT2	m_posEnd;		// Position of the end point of the line segment representing line of the node.
};
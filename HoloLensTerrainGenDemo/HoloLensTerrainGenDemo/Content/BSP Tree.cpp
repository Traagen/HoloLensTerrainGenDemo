#include "pch.h"
#include "BSP Tree.h"

// Default Constructor
BSPNode::BSPNode() {
	m_childLeft = nullptr;
	m_childRight = nullptr;
	m_parent = nullptr;
	m_posStart.x = m_posStart.y = m_posEnd.x = m_posEnd.y = 0.0f;
}

// Constructor to create new Node with supplied parent node.
BSPNode::BSPNode(BSPNode *parent) {
	m_childLeft = nullptr;
	m_childRight = nullptr;
	m_parent = parent;
	m_posStart.x = m_posStart.y = m_posEnd.x = m_posEnd.y = 0.0f;
}

// Destructor
BSPNode::~BSPNode() {
	if (m_childLeft) delete m_childLeft;
	if (m_childRight) delete m_childRight;
	if (m_parent) m_parent = nullptr;
}

// Create the left child;
BSPNode *BSPNode::CreateLeftChild() {
	m_childLeft = new BSPNode(this);
	return m_childLeft;
}

// Create the right child;
BSPNode *BSPNode::CreateRightChild() {
	m_childRight = new BSPNode(this);
	return m_childRight;
}
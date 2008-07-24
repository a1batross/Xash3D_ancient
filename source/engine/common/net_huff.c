//=======================================================================
//			Copyright XashXT Group 2007 ©
//	net_huff.c - Huffman compression routines for network bitstream
//=======================================================================

#include "common.h"
#include "builtin.h"

#define VALUE(a)			((int   )(a))
#define NODE(a)			((void *)(a))

#define NODE_START			NODE(  1)
#define NODE_NONE			NODE(256)
#define NODE_NEXT			NODE(257)

#define NOT_REFERENCED		256

#define HUFF_TREE_SIZE		7175
typedef void			*tree_t[HUFF_TREE_SIZE];

//
// pre-defined frequency counts for all bytes [0..255]
//


// static Huffman tree
static tree_t	huffTree;

// received from MSG_* code
static int	huffBitPos;
static bool	huffInit = false;

/*
=======================================================================================

  HUFFMAN TREE CONSTRUCTION

=======================================================================================
*/

/*
============
Huff_PrepareTree
============
*/
static _inline void Huff_PrepareTree( tree_t tree )
{
	void	**node;
	
	memset( tree, 0, sizeof( tree_t ));
	
	// create first node
	node = &tree[263];
	VALUE( tree[0] )++;

	node[7] = NODE_NONE;
	tree[2] = node;
	tree[3] = node;
	tree[4] = node;
	tree[261] = node;
}



/*
============
Huff_GetNode
============
*/
static _inline void **Huff_GetNode( void **tree )
{
	void	**node;
	int	value;

	node = tree[262];
	if( !node )
	{
		value = VALUE( tree[1] )++;
		node = &tree[value + 6407];
		return node;
	}

	tree[262] = node[0];
	return node;
}

/*
============
Huff_Swap
============
*/
static _inline void Huff_Swap( void **tree1, void **tree2, void **tree3 )
{
	void **a, **b;

	a = tree2[2];
	if( a )
	{
		if( a[0] == tree2 )
			a[0] = tree3;
		else a[1] = tree3;
	}
	else tree1[2] = tree3;
	b = tree3[2];

	if( b )
	{
		if( b[0] == tree3 )
		{
			b[0] = tree2;
			tree2[2] = b;
			tree3[2] = a;
			return;
		}

		b[1] = tree2;
		tree2[2] = b;
		tree3[2] = a;
		return;
	}

	tree1[2] = tree2;
	tree2[2] = NULL;
	tree3[2] = a;
}

/*
============
Huff_SwapTrees
============
*/
static _inline void Huff_SwapTrees( void **tree1, void **tree2 )
{
	void **temp;

	temp = tree1[3];
	tree1[3] = tree2[3];
	tree2[3] = temp;

	temp = tree1[4];
	tree1[4] = tree2[4];
	tree2[4] = temp;

	if( tree1[3] == tree1 )
		tree1[3] = tree2;

	if( tree2[3] == tree2 )
		tree2[3] = tree1;

	temp = tree1[3];
	if( temp ) temp[4] = tree1;

	temp = tree2[3];
	if( temp ) temp[4] = tree2;

	temp = tree1[4];
	if( temp ) temp[3] = tree1;

	temp = tree2[4];
	if( temp ) temp[3] = tree2;
}

/*
============
Huff_DeleteNode
============
*/
static _inline void Huff_DeleteNode( void **tree1, void **tree2 )
{
	tree2[0] = tree1[262];
	tree1[262] = tree2;
}

/*
============
Huff_IncrementFreq_r
============
*/
static void Huff_IncrementFreq_r( void **tree1, void **tree2 )
{
	void **a, **b;

	if( !tree2 ) return;

	a = tree2[3];
	if( a )
	{
		a = a[6];
		if( a == tree2[6] )
		{
			b = tree2[5];
			if( b[0] != tree2[2] )
				Huff_Swap( tree1, b[0], tree2 );
			Huff_SwapTrees( b[0], tree2 );
		}
	}

	a = tree2[4];
	if( a && a[6] == tree2[6] )
	{
		b = tree2[5];
		b[0] = a;
	}
	else
	{
		a = tree2[5];
		a[0] = 0;
		Huff_DeleteNode( tree1, tree2[5] );
	}

	
	VALUE( tree2[6] )++;
	a = tree2[3];
	if( a && a[6] == tree2[6] )
	{
		tree2[5] = a[5];
	}
	else
	{
		a = Huff_GetNode( tree1 );
		tree2[5] = a;
		a[0] = tree2;
	}

	if( tree2[2] )
	{
		Huff_IncrementFreq_r( tree1, tree2[2] );
	
		if( tree2[4] == tree2[2] )
		{
			Huff_SwapTrees( tree2, tree2[2] );
			a = tree2[5];

			if( a[0] == tree2 ) a[0] = tree2[2];
		}
	}
}

/*
============
Huff_AddReference

Insert 'ch' into the tree or increment it's frequency
============
*/
static void Huff_AddReference( void **tree, int ch )
{
	void **a, **b, **c, **d;
	int value;

	ch &= 255;
	if( tree[ch + 5] )
	{
		Huff_IncrementFreq_r( tree, tree[ch + 5] );
		return; // already added
	}

	value = VALUE( tree[0] )++;
	b = &tree[value * 8 + 263];

	value = VALUE( tree[0] )++;
	a = &tree[value * 8 + 263];

	a[7] = NODE_NEXT;
	a[6] = NODE_START;
	d = tree[3];
	a[3] = d[3];
	if( a[3] )
	{
		d = a[3];
		d[4] = a;
		d = a[3];
		if( d[6] == NODE_START )
		{
			a[5] = d[5];
		}
		else
		{
			d = Huff_GetNode( tree );
			a[5] = d;
			d[0] = a;
		}
	}
	else
	{
		d = Huff_GetNode( tree );
		a[5] = d;
		d[0] = a;

	}
	
	d = tree[3];
	d[3] = a;
	a[4] = tree[3];
	b[7] = NODE( ch );
	b[6] = NODE_START;
	d = tree[3];
	b[3] = d[3];
	if( b[3] )
	{
		d = b[3];
		d[4] = b;
		if( d[6] == NODE_START )
		{
			b[5] = d[5];
		}
		else
		{
			d = Huff_GetNode( tree );
			b[5] = d;
			d[0] = a;
		}
	}
	else
	{
		d = Huff_GetNode( tree );
		b[5] = d;
		d[0] = b;
	}

	d = tree[3];
	d[3] = b;
	b[4] = tree[3];
	b[1] = NULL;
	b[0] = NULL;
	d = tree[3];
	c = d[2];
	if( c )
	{
		if( c[0] == tree[3] )
			c[0] = a;
		else c[1] = a;
	}
	else tree[2] = a;

	a[1] = b;
	d = tree[3];
	a[0] = d;
	a[2] = d[2];
	b[2] = a;
	d = tree[3];
	d[2] = a;
	tree[ch + 5] = b;

	Huff_IncrementFreq_r( tree, a[2] );
}

/*
=======================================================================================

  BITSTREAM I/O

=======================================================================================
*/

/*
============
Huff_EmitBit

Put one bit into buffer
============
*/
static _inline void Huff_EmitBit( int bit, byte *buffer )
{
	if(!(huffBitPos & 7)) buffer[huffBitPos >> 3] = 0;
	buffer[huffBitPos >> 3] |= bit << (huffBitPos & 7);
	huffBitPos++;
}

/*
============
Huff_GetBit

Read one bit from buffer
============
*/
static _inline int Huff_GetBit( byte *buffer )
{
	int bit = buffer[huffBitPos >> 3] >> (huffBitPos & 7);
	huffBitPos++;
	return (bit & 1);
}

/*
============
Huff_EmitPathToByte
============
*/
static _inline void Huff_EmitPathToByte( void **tree, void **subtree, byte *buffer )
{
	if( tree[2] ) Huff_EmitPathToByte( tree[2], tree, buffer );
	if( !subtree ) return;

	// emit tree walking control bits
	if( tree[1] == subtree )
		Huff_EmitBit( 1, buffer );
	else Huff_EmitBit( 0, buffer );
}

/*
============
Huff_GetByteFromTree

Get one byte using dynamic or static tree
============
*/
static _inline int Huff_GetByteFromTree( void **tree, byte *buffer )
{
	if( !tree ) return 0;

	// walk through the tree until we get a value
	while( tree[7] == NODE_NEXT )
	{
		if( !Huff_GetBit( buffer ))
			tree = tree[0];
		else tree = tree[1];
		if( !tree ) return 0;
	}
	return VALUE( tree[7] );
}

/*
============
Huff_EmitByteDynamic

Emit one byte using dynamic tree
============
*/
static void Huff_EmitByteDynamic( void **tree, int value, byte *buffer )
{
	void **subtree;
	int i;

	// if byte was already referenced, emit path to it
	subtree = tree[value + 5];
	if( subtree )
	{
		if( subtree[2] )
			Huff_EmitPathToByte( subtree[2], subtree, buffer );
		return;
	}

	// byte was not referenced, just emit 8 bits
	Huff_EmitByteDynamic( tree, NOT_REFERENCED, buffer );

	for( i = 7; i >= 0; i-- )
	{
		Huff_EmitBit( (value >> i) & 1, buffer );
	}

}

/*
=======================================================================================

  PUBLIC INTERFACE

=======================================================================================
*/

/*
============
Huff_CompressPacket

Compress message using dynamic Huffman tree,
beginning from specified offset
============
*/
void Huff_CompressPacket( sizebuf_t *msg, int offset )
{
	tree_t	tree;
	byte	buffer[MAX_MSGLEN];
	byte	*data;
	int	outLen;
	int	i, inLen;

	data = msg->data + offset;
	inLen = msg->cursize - offset;	
	if( inLen <= 0 || inLen >= MAX_MSGLEN )
		return;

	Huff_PrepareTree( tree );

	buffer[0] = inLen >> 8;
	buffer[1] = inLen & 0xFF;
	huffBitPos = 16;

	for( i = 0; i < inLen; i++ )
	{
		Huff_EmitByteDynamic( tree, data[i], buffer );
		Huff_AddReference( tree, data[i] );
	}
	
	outLen = (huffBitPos >> 3) + 1;
	msg->cursize = offset + outLen;
	Mem_Copy( data, buffer, outLen );

}

/*
============
Huff_DecompressPacket

Decompress message using dynamic Huffman tree,
beginning from specified offset
============
*/
void Huff_DecompressPacket( sizebuf_t *msg, int offset )
{
	tree_t	tree;
	byte	buffer[MAX_MSGLEN];
	byte	*data;
	int	outLen;
	int	inLen;
	int	ch, i, j;

	data = msg->data + offset;
	inLen = msg->cursize - offset;
	if( inLen <= 0 ) return;

	Huff_PrepareTree( tree );

	outLen = (data[0] << 8) + data[1];
	huffBitPos = 16;
	
	if( outLen > msg->maxsize - offset )
		outLen = msg->maxsize - offset;

	for( i = 0; i < outLen; i++ )
	{
		if((huffBitPos >> 3) > inLen )
		{
			buffer[i] = 0;
			break;
		}

		ch = Huff_GetByteFromTree( tree[2], data );

		if( ch == NOT_REFERENCED )
		{
			ch = 0; // just read 8 bits
			for( j = 0; j < 8; j++ )
			{
				ch <<= 1;
				ch |= Huff_GetBit( data );
			}
		}
		buffer[i] = ch;
		Huff_AddReference( tree, ch );
	}

	msg->cursize = offset + outLen;
	Mem_Copy( data, buffer, outLen );
}

/*
============
Huff_Init
============
*/
void Huff_Init( void )
{
	int	i, j;

	if( huffInit ) return;

	// build empty tree
	Huff_PrepareTree( huffTree );

	// add all pre-defined byte references
	for( i = 0; i < 256; i++ )
		for( j = 0; j < huff_tree[i]; j++ )
			Huff_AddReference( huffTree, i );
	huffInit = true;
}
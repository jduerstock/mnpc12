/*=============================================================================

			         The Microcom MNP Library
					(Microsoft C Version)

-------------------------------------------------------------------------------

	       MNPMISC - Miscellaneous MNP Support Routines

==========================================================================*/

/* Header files */
#include <mnpdat.h>

/***************************************************************************
*
*	BUFFER MANAGEMENT - INITIALIZE THE ARRAY OF STRUCTURES CONTROLLING
*				THE BUFFERS
*
*	Synopsis:	init_blst(buff, len)
*			struct buffer *b_struct;
*			int len;
*			char *buff;
*
*			returns:
*
****************************************************************************/


SIGN_16 init_blst(b_struct, len, buff)

struct BUFFER *b_struct;
USIGN_16 len;
USIGN_8 *buff;

{
register USIGN_16 i;

for (i=0; i < b_struct->num - 1;)
	{
	b_struct->list[i].mark = 0;
	b_struct->list[i].bptr = buff + (len * i);
	b_struct->list[i].next_b = &b_struct->list[++i];
	}
b_struct->list[i].mark = 0;
b_struct->list[i].bptr = buff + (len * i);
b_struct->free = b_struct->list[i].next_b = b_struct->list;

b_struct->used = 0;
b_struct->used_lst = 0;

return;
}

/***************************************************************************
*
*	BUFFER MANAGEMENT - GET POINTER TO A FREE BUFFER
*
*	Synopsis:
*
*
*			returns: SUCCESS
*				 FAILURE - no free buffer
*
***************************************************************************/

SIGN_16 get_b(b_struct, bl_struct)

struct BUFFER *b_struct;
struct BLST **bl_struct;

{
if ((USIGN_16)(*bl_struct = b_struct->free))
	{
	(*bl_struct)->mark = 1;
	if (++b_struct->used == b_struct->num)
		b_struct->free = 0;
	else
		b_struct->free = b_struct->free->next_b;
	return (SUCCESS);
	}
else
	return (FAILURE);
}

/***************************************************************************
*
*	BUFFER MANAGEMENT - RETURN A BUFFER TO THE FREE POOL
*
*	Synopsis:
*
*
*
*			returns: 0: success
*
****************************************************************************/


SIGN_16 ret_b(b_struct, bl_struct)

struct BUFFER *b_struct;
struct BLST *bl_struct;

{
if (bl_struct->mark)
	{
	bl_struct->mark = 0;
	b_struct->used--;
	if ((USIGN_16)b_struct->free == 0)
		b_struct->free = bl_struct;
	}
return (SUCCESS);
}



/***************************************************************************
*
*	BUFFER MANAGEMENT - RESET THE WHOLE BUFFER POOL TO BE FREE
*
*	Synopsis:
*
*
*
****************************************************************************/

void reset_blst(b_struct)

struct BUFFER *b_struct;

{
register USIGN_16 i;

for (i=0; i < b_struct->num; i++)
	b_struct->list[i].mark = b_struct->list[i].len = 0;  /* mark all as free */

b_struct->used = 0;
b_struct->used_lst = 0;
b_struct->free = b_struct->list;
}


/***************************************************************************
*
*
*
*
****************************************************************************/

USIGN_16 min (a, b)

USIGN_16  a, b;
{
return ((a) < (b) ? (a): (b));
}

USIGN_16 max (a, b)

USIGN_16 a, b;
{
return ((a) > (b) ? (a): (b));
}

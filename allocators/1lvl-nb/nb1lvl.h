/**		      
* This is free software; 
* You can redistribute it and/or modify this file under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This file is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* You should have received a copy of the GNU General Public License along with
* this file; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
* 
* Copyright (c) 2017
* 
* 
* Romolo Marotta 
* Mauro Ianni
* Andrea Scarselli
* 
* 
* This file implements the non blocking buddy system (1lvl).
* 
*/

#ifndef __1LVL_ALLOC__
#define __1LVL_ALLOC__

typedef struct _node{
    volatile unsigned long long val; //per i bit etc;
    char pad[44];
    unsigned int mem_start; //spiazzamento all'interno dell'overall_memory
    unsigned int mem_size;
    unsigned int pos; //posizione all'interno dell'array "tree"
} node;

#include "nballoc.h"

#endif

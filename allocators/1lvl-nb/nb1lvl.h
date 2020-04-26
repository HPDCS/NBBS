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
* Copyright (c) 2017 - 2020
* 
* 
* Romolo Marotta  (Contact author)
* Mauro Ianni
* Andrea Scarselli
* 
* 
* This file provide the struct for a node of the NBBS in the 1lvl configuration
* 
* 
* 
* 
*/

#ifndef __1LVL_ALLOC__
#define __1LVL_ALLOC__

typedef struct _node{
    volatile unsigned long long val;    // this maintain the state of a node;
    char pad[56];                       // padding
} node;

#include "nballoc.h"

#endif

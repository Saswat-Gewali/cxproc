/*
  cxproc - Configurable Xml PROCessor

  Copyright (C) 2006..2020 by Alexander Tenbusch

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*!
*/
int
imageTest(cxpContextPtr pccArg)
{
  int i;
  int n_ok;

  n_ok=0;
  i=0;

#ifdef HAVE_LIBMAGICK
#else
  printf("Skip TEST in '%s'\n\n",__FILE__);
#endif

  return (i - n_ok);
}
/* end of imageTest() */

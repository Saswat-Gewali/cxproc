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

extern BOOL_T
resNodeListParse(resNodePtr prnArg, int iArgDepth, const pcre2_code *re_match);

extern void
resNodeListFree(resNodePtr prnArg);

extern BOOL_T
resNodeListUnlink(resNodePtr prnArg);

extern BOOL_T
resNodeListUnlinkDescendants(resNodePtr prnArg);

extern resNodePtr
resNodeListFindChild(resNodePtr prnArg, xmlChar *pucName);

extern resNodePtr
resNodeListFind(resNodePtr prnArg, xmlChar *pucArgPath, xmlChar *pucPattern);

extern resNodePtr
resNodeListFindPath(resNodePtr prnArg, xmlChar *pucArgPath);

extern resNodePtr
resNodeListFindPathNext(resNodePtr prnArg, xmlChar *pucArgPath);

extern xmlNodePtr
resNodeListToDOM(resNodePtr prnArg, int iLevelVerboseArg);

extern xmlChar *
resNodeListToSQL(resNodePtr prnArg, int iLevelVerboseArg);

extern xmlChar *
resNodeListToJSON(resNodePtr prnArg, int iLevelVerboseArg);

extern xmlChar *
resNodeListToXml(resNodePtr prnArg, int iLevelVerboseArg);

extern xmlChar *
resNodeListToPlain(resNodePtr prnArg, int iLevelVerboseArg);

#ifdef TESTCODE
int
resNodeTestList(void);
#endif

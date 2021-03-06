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
pieCalendarTest(cxpContextPtr pccArg)
{
  int n_ok;
  int i;

  n_ok=0;
  i=0;

  if (SKIPTEST) {
    xmlNodePtr pndTask;

    i++;
    printf("TEST %i in '%s:%i': calculate delta days = ",i,__FILE__,__LINE__);

    pndTask = xmlNewNode(NULL,NAME_PIE_TASK);
    xmlSetProp(pndTask, BAD_CAST "date", BAD_CAST"2012*w2mon");
    //AddAttributeDayDiff(pndTask);
    n_ok++;
    printf("OK\n");
    xmlFreeNode(pndTask);
  }

  if (RUNTEST) {
    xmlNodePtr pndMakeCalendar;
    xmlNodePtr pndCol;
    xmlNodePtr pndChild;
    xmlNodePtr pndPie;
    xmlNodePtr pndPieImport;
    xmlNodePtr pndMonth;
    xmlDocPtr pdocCalendar;
    xmlChar *pucData = BAD_CAST "00000000 %a %d.%m. (%j)\n0000*w99mon KW %V/%Y\n0000*w99 %V/%Y\n000000 Middle of the Month %Y-%m\n0000 Middle of the Year %Y";
    xmlNodePtr pndDay;
    xmlChar *pucT = NULL;

    i++;
    printf("TEST %i in '%s:%i': build a pie calendar = ", i, __FILE__, __LINE__);

    pndMakeCalendar = xmlNewNode(NULL, NAME_CALENDAR);
    //xmlSetProp(pndMakeCalendar, BAD_CAST "year", BAD_CAST"2015");
    xmlSetProp(pndMakeCalendar, BAD_CAST "year", BAD_CAST"content");
    //xmlSetProp(pndMakeCalendar, BAD_CAST "type", BAD_CAST"year");
    xmlSetProp(pndMakeCalendar, BAD_CAST "coordinate", BAD_CAST"5225+01234/");
    xmlSetProp(pndMakeCalendar, BAD_CAST "timezone", BAD_CAST"CET");

    pndCol = xmlNewChild(pndMakeCalendar, NULL, NAME_COL, NULL);
    xmlSetProp(pndCol, BAD_CAST"id", BAD_CAST"test");
    pndChild = xmlNewChild(pndCol, NULL, NAME_XML, NULL);
    pndPie = xmlNewChild(pndChild, NULL, NAME_PIE, NULL);
    pndPieImport = xmlNewChild(pndPie, NULL, NAME_PIE_IMPORT, pucData);
    xmlSetProp(pndPieImport, BAD_CAST "type", BAD_CAST"log");

    pndCol = xmlNewChild(pndMakeCalendar, NULL, NAME_COL, NULL);
    xmlSetProp(pndCol, BAD_CAST"id", BAD_CAST"content");
    pndChild = xmlNewChild(pndCol, NULL, NAME_XML, NULL);
    pndPie = xmlNewChild(pndChild, NULL, NAME_PIE, NULL);
    pndPieImport = xmlNewChild(pndPie, NULL, NAME_PIE_IMPORT, NULL);
    xmlSetProp(pndPieImport, BAD_CAST "name", BAD_CAST DATAPREFIX "Calendars/TestContent.pie");

    //domPutNodeString(stderr, BAD_CAST "pndMakeCalendar", pndMakeCalendar);

    if ((pdocCalendar = calProcessCalendarNode(pndMakeCalendar, pccArg)) == NULL) {
      printf("Error calProcessCalendarNode()\n");
    }
#if 0
    else if ((pndMonth = GetCalendarNodeMonth(xmlDocGetRootElement(pdocCalendar), 2015, 2)) == NULL) {
      printf("Error GetCalendarNodeMonth()\n");
    }
    else if ((pndDay = domGetFirstChild(pndMonth, BAD_CAST"day")) == NULL) {
      printf("Error domGetFirstChild()\n");
    }
    else if ((pucT = SubstituteFormatStr(pndDay->children, BAD_CAST"TEST %Y-%m-%d AAA %G %SR %SS FF %a (%j) WWW %MOON VVV")) == NULL) {
      printf("Error 1 SubstituteFormatStr()\n");
    }
    else if (xmlStrEqual(pucT, BAD_CAST"TEST 2015-02-01 AAA %G 7:51 16:55 FF sun (32) WWW  VVV") == FALSE) {
      printf("Error 2 SubstituteFormatStr()\n");
    }
#endif
    else {
      n_ok++;
      printf("OK\n");
    }
    //domPutDocString(stderr, BAD_CAST "pdocCalendar", pdocCalendar);
    xmlFree(pucT);
    xmlFreeDoc(pdocCalendar);
    xmlFreeNode(pndMakeCalendar);
  }


  if (RUNTEST) {
    xmlDocPtr pdocTest = NULL;

    i++;
    printf("TEST %i in '%s:%i': calAddAttributeDayDiff() = ", i, __FILE__, __LINE__);

    if ((pdocTest = xmlParseFile(DATAPREFIX "Documents/TestContent.pie")) == NULL) {
      printf("Error xmlParseFile()\n");
    }
    else if (calAddAttributeDayDiff(pdocTest) != pdocTest) {
      printf("Error calAddAttributeDayDiff()\n");
    }
    else {
      n_ok++;
      printf("OK\n");
    }
    //domPutDocString(stderr, BAD_CAST "calAddAttributeDayDiff() result", pdocTest);
    xmlFreeDoc(pdocTest);
  }


  if (RUNTEST) {
    xmlDocPtr pdocTest = NULL;

    i++;
    printf("TEST %i in '%s:%i': calAddAttributeDayDiff() = ", i, __FILE__, __LINE__);

    if ((pdocTest = pieParseFile(BAD_CAST DATAPREFIX "Documents/TestContent.txt",pccArg)) == NULL) {
      printf("Error pieParseFile()\n");
    }
    else if (calAddAttributeDayDiff(pdocTest) != pdocTest) {
      printf("Error calAddAttributeDayDiff()\n");
    }
    else {
      n_ok++;
      printf("OK\n");
    }
    //domPutDocString(stderr, BAD_CAST "calAddAttributeDayDiff() result", pdocTest);
    xmlFreeDoc(pdocTest);
  }


  if (RUNTEST) {
    xmlDocPtr pdocTest = NULL;
    xmlDocPtr pdocDir = NULL;
    xmlNodePtr pndT;
    xmlNodePtr pndPie;
    xmlNodePtr pndDir;
    cxpContextPtr pccT;

    i++;
    printf("TEST %i in '%s:%i': include directory listing in calendar = ", i, __FILE__, __LINE__);

    pndPie = xmlNewNode(NULL, NAME_PIE);

    if ((pccT = cxpCtxtDup(pccArg)) == NULL) {
      printf("Error cxpCtxtDup()\n");
    }
    else if ((pndDir = xmlNewChild(pndPie, NULL, NAME_DIR, NULL)) == NULL
      || xmlSetProp(pndDir, BAD_CAST"depth", BAD_CAST"1") == FALSE
      || xmlSetProp(pndDir, BAD_CAST"verbosity", BAD_CAST"3") == FALSE) {
      printf("Error xmlNewChild()\n");
    }
    else if ((pndT = xmlNewChild(pndDir, NULL, NAME_DIR, NULL)) == NULL
      || xmlSetProp(pndT, BAD_CAST"name", BAD_CAST DATAPREFIX) == FALSE) {
      printf("Error xmlNewChild()\n");
    }
    else if ((pdocDir = dirProcessDirNode(pndDir, NULL, pccT)) == NULL) {
      printf("Error dirProcessDirNode()\n");
    }
    else if ((pdocTest = calProcessDoc(pdocDir, pccT)) == NULL) {
      printf("Error calProcessDoc()\n");
    }
    else {
      n_ok++;
      printf("OK\n");
    }
    //domPutDocString(stderr, BAD_CAST "calProcessDoc() result", pdocTest);
    cxpCtxtFree(pccT);
    xmlFreeDoc(pdocDir);
    xmlFreeDoc(pdocTest);
    xmlFreeNode(pndPie);
  }


  printf("Result in '%s': %i/%i OK\n\n", __FILE__, n_ok, i);

  return (i - n_ok);
}
/* end of calTest() */

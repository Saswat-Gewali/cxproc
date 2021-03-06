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

#include <math.h>

#include <libxml/tree.h>

#include <sunriset/sunriset.h>

/* 
 */
#include "basics.h"
#include "utils.h"
#include <res_node/res_node.h>
#include <cxp/cxp.h>
#include <cxp/cxp_dir.h>
#include "dom.h"
#include <pie/pie_text.h>
#include <pie/pie_calendar.h>


typedef enum {
  PIE_CALENDAR_MDAY, 
  PIE_CALENDAR_WDAY, 
  PIE_CALENDAR_MDAY_HOUR, 
  PIE_CALENDAR_WDAY_HOUR, 
  PIE_CALENDAR_WEEK, 
  PIE_CALENDAR_MONTH, 
  PIE_CALENDAR_YEAR
} pieCalendarType;

/*! structure
 */
typedef struct {
  xmlDocPtr pdocCalendar; /*!< pointer to DOM of result calendar DOM */
  xmlNodePtr pndCalendarRoot;
  xmlNodePtr mpndDay[256 * 256]; /*!< array of pointers to day nodes of pdocCalendar (performance vector) */
  pieCalendarElementPtr pceFirst;
  pieCalendarType eType;
  
  unsigned int ciEntries;
  int *pmiYear; /*!< array of requiered years, derived from attributes in pdocSource */

  BOOL_T fCoordinate;
  double dLatitude;
  double dLongitude;

  int iTimezone;	/*!< numerical ID for timezone (UTC by default) */
  int iTimezoneOffset;  /*!< offset to UTC in minutes */
} pieCalendar;

typedef pieCalendar *pieCalendarPtr;

/* Global 
 */
const xmlChar *moy[] = {
  BAD_CAST "January",
  BAD_CAST "February",
  BAD_CAST "March",
  BAD_CAST "April",
  BAD_CAST "May",
  BAD_CAST "June",
  BAD_CAST "July",
  BAD_CAST "August",
  BAD_CAST "September",
  BAD_CAST "October",
  BAD_CAST "November",
  BAD_CAST "December",
  NULL
};

const xmlChar *moy_de[] = {
  BAD_CAST "Januar",
  BAD_CAST "Februar",
  BAD_CAST "Maerz",
  BAD_CAST "April",
  BAD_CAST "Mai",
  BAD_CAST "Juni",
  BAD_CAST "Juli",
  BAD_CAST "August",
  BAD_CAST "September",
  BAD_CAST "Oktober",
  BAD_CAST "November",
  BAD_CAST "Dezember",
  NULL
};

/*!\todo
BOOL_T
CalendarSwitchTimezone(pieCalendarPtr pCalendarArg, pie_tz_t tzArg);
 */

static void
GetYearArray(pieCalendarPtr pCalendarArg);

static BOOL_T
AddTreeYear(pieCalendarPtr pCalendarArg, int year);

static void
GetYearMinMax(pieCalendarPtr pCalendarArg, int *year_min, int *year_max);

static BOOL_T
AddYears(pieCalendarPtr pCalendarArg);

static xmlNodePtr
FindCalendarElementCol(xmlNodePtr pndArgParent, xmlChar *pucArgIdCol, xmlNodePtr pndArgInsert);

static void
InsertCalendarElementEat(pieCalendarPtr pCalendarArg, pieCalendarElementPtr pceArg, xmlNodePtr pndArg);

static void
CalendarUpdate(pieCalendarPtr pCalendarArg);

static void
AddAttributeDayDiff(pieCalendarPtr pCalendarArg);

static BOOL_T
AddAttributeTime(xmlNodePtr pndArg);

static BOOL_T
RegisterDateNodes(pieCalendarPtr pCalendarArg, xmlChar *pucArg);

static BOOL_T
ParseDates(pieCalendarPtr pCalendarArg);

static BOOL_T
ProcessCalendarColumns(pieCalendarPtr pCalendarArg, cxpContextPtr pccArg);

static void
CalendarColumnsFree(pieCalendarPtr pCalendarArg);

static void
CalendarFree(pieCalendarPtr pCalendarArg);

static pieCalendarPtr
CalendarNew(void);

static pieCalendarPtr
CalendarSetup(xmlNodePtr pndMakeCalendar, cxpContextPtr pccArg);

static xmlNodePtr
GetCalendarNodeYear(xmlNodePtr pndArg, unsigned int uiArgYear);

static xmlNodePtr
GetCalendarNodeMonth(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth);

static xmlNodePtr
GetCalendarNodeDay(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth, unsigned int uiArgDay);

static xmlNodePtr
GetCalendarNodeDayHour(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth, unsigned int uiArgDay, unsigned int uiArgHour);

static xmlNodePtr
GetCalendarNodeWeek(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgWeek);

static xmlChar *
SubstituteFormatStr(xmlNodePtr pndContext, xmlChar *fmt);

static BOOL_T
SubstituteFormat(xmlNodePtr pndArg);

static xmlNodePtr
GetCalendarNodeNext(const xmlNodePtr pndI, int distance);

static BOOL_T
IsFullMoonConway(int year, int month, int day);

#ifdef DEBUG
BOOL_T
PrintCalendarSetup(pieCalendarPtr pCalendarArg, cxpContextPtr pccArg);
#endif

/*! Searches in a CALENDAR DOM for the next node with same name like pndArg.

\param pndArg start node
\param distance between result and pndArg

Both directions are supported. Distance values lower than zero means
backward searching.
*/
xmlNodePtr
GetCalendarNodeNext(const xmlNodePtr pndArg, int distance)
{
  BOOL_T fForward = TRUE;
  xmlNodePtr pndN;

  /*!\todo problems when a date definition starts before begin of
    calendar DOM (maybe projection from past to present)

    2005*w3mon:7 nor result in calendar starting 2006

  */

  if (pndArg && pndArg->type == XML_ELEMENT_NODE) {

    if (IS_NODE_PIE_WEEK(pndArg)) {
      /* pndArg is a WEEK element 
       */
      if (distance == 0) {
	/*! nearest distance is reached */
	return pndArg;
      }
      else if (distance > 0) {  /* search forward */
	distance--;
      }
      else {  /*!\todo search backward */
	fForward = FALSE;
	distance++;
      }
      if (fForward) {
	return GetCalendarNodeNext(domGetNextNode(pndArg,NULL),distance);
      }
      else {
	assert(FALSE);		/*!\todo backward movement */
      }
    }
    else if (IS_NODE_PIE_DAY(pndArg)) {
      /* pndArg is a DAY element 
       */
      if (distance == 0) {
	/*! nearest distance is reached */
	return pndArg;
      }
      else if (distance > 0) {  /* search forward */
	distance--;
      }
      else {  /*!\todo search backward */
	fForward = FALSE;
	distance++;
      }

      if (fForward) {
	if ((pndN = domGetNextNode(pndArg,NULL))) {
	  return GetCalendarNodeNext(pndN,distance);
	}
      }
      else {
	if (pndArg->prev) {	/*!\todo use prev_element(pndArg,NULL) */
	  /*!\bug when element in previuos period, pndArg->prev==NULL */
	  return GetCalendarNodeNext(pndArg->prev,distance);
 	}
      }
      /*
	no next/prev DAY node in this MONTH found
       */
      if (pndArg->parent) {
	xmlNodePtr pndPn;
	xmlNodePtr pndPnc;
	if (fForward) {
	  /* search for a pndArg->parent->next->children */
	  for (pndPn=pndArg->parent->next; pndPn; pndPn=pndPn->next) {
	    if ((pndPnc = domGetFirstChild(pndPn,BAD_CAST pndArg->name))) {
	      /* use first DAY in next MONTH */
	      return GetCalendarNodeNext(pndPnc,distance);
	    }
	  }
	}
	else {
	  for (pndPn=pndArg->parent->prev; pndPn; pndPn=pndPn->prev) {
	    if (pndPn->last) {
	      /* use last DAY in prev MONTH */
	      return GetCalendarNodeNext(pndPn->last,distance);
	    }
	  }
	}
	/*
	  no next/prev MONTH node in this YEAR found
	  */
	if (pndArg->parent->parent) {
	  xmlNodePtr pndPpn;
	  if (fForward) {
	    /* else search for a pndArg->parent->parent->next->children->children */
	    for (pndPpn = pndArg->parent->parent->next; pndPpn; pndPpn = pndPpn->next) {
	      if (pndPpn->children && pndPpn->children->children) {
		/* use first DAY in next MONTH */
		return GetCalendarNodeNext(pndPpn->children->children, distance);
	      }
	    }
	  }
	  else {
	    for (pndPpn = pndArg->parent->parent->prev; pndPpn; pndPpn = pndPpn->prev) {
	      if (pndPpn->last && pndPpn->last->last) {
		/* use last DAY in last MONTH in prev YEAR */
		return GetCalendarNodeNext(pndPpn->last->last, distance);
	      }
	    }
	  }
	}
      }
    }
    else {
      /* other elements from recursive calls */
    }
  }
  return NULL;
}
/* End of GetCalendarNodeNext() */


/*! sets the lower and upper values for year, according to registered calendar elements
 */
void
GetYearMinMax(pieCalendarPtr pCalendarArg, int *year_min, int *year_max)
{
  pieCalendarElementPtr pceT;

  for (pceT = pCalendarArg->pceFirst; pceT; pceT = pceT->pNext) {
    if (pceT->iYear > 0) {
      if (pceT->iYear < *year_min) {
	*year_min = pceT->iYear;
	if (*year_max == 0) {
	  *year_max = *year_min;
	}
      }
      else if (pceT->iYear > *year_max) {
	*year_max = pceT->iYear;
	if (*year_min == 2999) {
	  *year_min = *year_max;
	}
      }
    }
  }
}
/* End of GetYearMinMax() */


/*! \return an array of year numbers

syntax of \@YEAR

"2007"
"2005..2007"

*/
BOOL_T
AddYears(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
    GetYearArray(pCalendarArg);
    if (pCalendarArg->pmiYear) {
      int *piYear;
      
      for (piYear=pCalendarArg->pmiYear; *piYear != 0; piYear++) {
	if (*piYear < 1971) {
	  PrintFormatLog(1,"Ignore year '%i', value to low", *piYear);
	}
	else if (*piYear > 2099) {
	  PrintFormatLog(1,"Ignore year '%i', value to high", *piYear);
	  break;
	}
	else if (AddTreeYear(pCalendarArg, *piYear)) {
	  /* OK */
	}
	else {
	  return FALSE;
	}
      }
    }
  }
  return TRUE;
}
/* End of AddYears() */


/*! Find the according parent col element to pndArgInsert, check for redundancy or create a new one

 \param pndArgParent pointer to according date element in calendar
 \param pucArgIdCol pointer to ID string of the required col element
 \param pndArgInsert pointer to node to insert

 \return pointer to parent node
*/
xmlNodePtr
FindCalendarElementCol(xmlNodePtr pndArgParent, xmlChar *pucArgIdCol, xmlNodePtr pndArgInsert)
{
  xmlNodePtr pndResult = NULL;

  if (pndArgParent != NULL && pndArgInsert != NULL) {
    xmlNodePtr pndCol;
    xmlNodePtr pndHour;
    xmlNodePtr pndChild;
    xmlChar *pucHour;

    if ((pndHour = domGetFirstChild(pndArgParent,NAME_PIE_HOUR))
	&& (pucHour = domGetAttributePtr(pndArgInsert, BAD_CAST "hour"))) {
      /*  */
      for ( ; pndHour != NULL; pndHour = domGetNextNode(pndHour,NAME_PIE_HOUR)) {
	/* step trough all existing hour elements */
	if (xmlStrEqual(pucHour,domGetAttributePtr(pndHour, BAD_CAST "nr"))) {
	  return FindCalendarElementCol(pndHour, pucArgIdCol, pndArgInsert);
	}
      }
    }
    else if (pucArgIdCol) {
      for (pndCol = domGetFirstChild(pndArgParent,NAME_COL);
	  pndCol != NULL && xmlStrcasecmp(pucArgIdCol,domGetAttributePtr(pndCol,BAD_CAST "idref"));
	  pndCol = domGetNextNode(pndCol,NAME_COL)) {
	/* step trough all existing col elements */
      }
      if (pndCol == NULL) {
	/* create one before */
	pndResult = xmlNewChild(pndArgParent, NULL, NAME_COL, NULL);
	xmlSetProp(pndResult, BAD_CAST "idref", pucArgIdCol);
      }
      else {
	pndResult = pndCol;
      }
    }
    else {
      pndResult = pndArgParent;
    }

    /* check if there is a sibling with same content
     */
    if (pndResult) {
      for (pndChild = pndResult->children;
	pndChild;
	pndChild = pndChild->next) {
	if (domNodesAreEqual(pndChild,pndArgInsert)) {
	  PrintFormatLog(4,"Ignore redundant entry '%s'",(unsigned char*)pndArgInsert->name);
	  return NULL;
	}
      }
    }
  }
  return pndResult;
}
/* End of FindCalendarElementCol() */


/*! inserts a node according to a calendar element data into calendar DOM, including iterations, set ID and symbolic dates

 \param pCalendarArg pointer to calendar
 \param pceArg pointer to calendar element to process
 \param pndArg pointer to a node to add to a parent node or freed!
*/
void
InsertCalendarElementEat(pieCalendarPtr pCalendarArg, pieCalendarElementPtr pceArg, xmlNodePtr pndArg)
{
  xmlNodePtr pndCol;

  assert(pCalendarArg != NULL);
  assert(pCalendarArg->pdocCalendar != NULL);
  assert(pceArg != NULL);
  assert(pndArg != NULL);

  if (pceArg->iAnchor > 0) {
    /* an anchor is specified */
    if (pCalendarArg->mpndDay[pceArg->iAnchor]) {
      int i;
      unsigned int iAnchorNew;

      for (i=0, iAnchorNew=pceArg->iAnchor; i < pceArg->iCount; i++) {
	/* do iteration of this calendar element */
	iAnchorNew += pceArg->iStep;
	if (pCalendarArg->mpndDay[iAnchorNew]) {
	  xmlNodePtr pndNew;

	  if ((pndCol = FindCalendarElementCol(pCalendarArg->mpndDay[iAnchorNew], pceArg->pucColId, pndArg)) != NULL
	    && (pndNew = xmlCopyNode(pndArg, 1)) != NULL) {
	    xmlSetProp(pndNew, BAD_CAST"idref", pceArg->pucId);
	    xmlAddChild(pndCol, pndNew);
	  }
	  else {
	    break;
	  }
	}
      }

      if ((pndCol = FindCalendarElementCol(pCalendarArg->mpndDay[pceArg->iAnchor], pceArg->pucColId, pndArg))) {
	xmlSetProp(pndArg,BAD_CAST"idref",pceArg->pucId);
	xmlAddChild(pndCol,pndArg);
      }
      else {
	xmlFreeNode(pndArg);
      }
    }
    else {
      xmlFreeNode(pndArg);
    }
  }
  else {
    /* no anchor specified */

    if (pceArg->iYear < 0) {
      /* ignoring invalid calendar elements */
      xmlFreeNode(pndArg);
    }
    else if (pceArg->iYear == 0) {
      /* iterate this element over all processed years */

      if (pCalendarArg->pmiYear) {
	pieCalendarElementPtr pceNew;
	int iLength = xmlStrlen(pceArg->pucDate) + 2;

	pceNew = CalendarElementDup(pceArg);
	if (pceNew) {
	  int *piYear;

	  pceNew->pucDate = BAD_CAST xmlMalloc(iLength);
	  for (piYear=pCalendarArg->pmiYear; piYear != NULL && *piYear > 1900; piYear++) {
	    xmlNodePtr pndNew;

	    xmlStrPrintf(pceNew->pucDate, iLength, "%4i%s", *piYear, (pceArg->pucDate + 4));
	    pceNew->pucSep = NULL;
	    if (ScanCalendarElementDate(pceNew) && (pndNew = xmlCopyNode(pndArg, 1)) != NULL) {
	      ScanDateIteration(pceNew);
	      InsertCalendarElementEat(pCalendarArg, pceNew, pndNew);
	    }
	    else {
	      break;
	    }
	  }
	  CalendarElementFree(pceNew);
	}
      }
      xmlFreeNode(pndArg);
    }
    else {
      xmlNodePtr pndParent;

      if (pceArg->iMonth < 0) {
	if (pceArg->iWeek < 0) {
	  /* this entry belongs to /calendar/year */
	  if (pCalendarArg->eType == PIE_CALENDAR_YEAR) {
	    pndParent = GetCalendarNodeYear(pCalendarArg->pndCalendarRoot,pceArg->iYear);
	    if (pndParent) {
	      xmlSetProp(pndArg,BAD_CAST"idref",pceArg->pucId);
	      pndCol = FindCalendarElementCol(pndParent,pceArg->pucColId,pndArg);
	      if (pndCol) {
		xmlAddChild(pndCol,pndArg);
	      }
	      else {
		xmlFreeNode(pndArg);
	      }
	    }
	  }
	  else {
	    pceArg->iAnchor = GetDayAbsolute(pceArg->iYear,6,30,-1-1,-1); /* map to day middle of year */
	    InsertCalendarElementEat(pCalendarArg, pceArg, pndArg);
	  }
	}
	else if (pceArg->iWeek == 99) {
	  /* iteration of day of month are stored at pceArg already, s. ScanDateIteration() */
	  xmlFreeNode(pndArg);
	}
	else {
	  /*  */
	  if (pceArg->iDayWeek < 0) {
	    /* this entry belongs to /calendar/year/week */
	    if (pCalendarArg->eType == PIE_CALENDAR_WEEK) {
	      pndParent = GetCalendarNodeWeek(pCalendarArg->pndCalendarRoot,pceArg->iYear,pceArg->iWeek);
	      if (pndParent) {
		xmlSetProp(pndArg,BAD_CAST"idref",pceArg->pucId);
		pndCol = FindCalendarElementCol(pndParent,pceArg->pucColId,pndArg);
		if (pndCol) {
		  xmlAddChild(pndCol,pndArg);
		}
		else {
		  xmlFreeNode(pndArg);
		}
	      }
	      else {
		xmlFreeNode(pndArg);
	      }
	    }
	    else {
	      pceArg->iAnchor = GetDayAbsolute(pceArg->iYear,-1,-1,pceArg->iWeek,3); /* map to 3rd day of week */
	      InsertCalendarElementEat(pCalendarArg, pceArg, pndArg);
	    }
	  }
	  else if (pceArg->iDayWeek == 7) {
	    /* iteration of day of month are stored at pceArg already, s. ScanDateIteration() */
	    xmlFreeNode(pndArg);
	  }
	  else {
	    /*  */
	    xmlFreeNode(pndArg);
	  }
	}
      }
      else if (pceArg->iMonth == 0) {
	/* iterate this element over all 12 months */

	pieCalendarElementPtr pceNew;
	int iLength = xmlStrlen(pceArg->pucDate) + 2;

	pceNew = CalendarElementDup(pceArg);
	if (pceNew) {
	  pceNew->pucDate = BAD_CAST xmlMalloc(iLength);
	  for (pceArg->iMonth=1; pceArg->iMonth < 13; pceArg->iMonth++) {
	    xmlNodePtr pndNew;

	    xmlStrPrintf(pceNew->pucDate, iLength, "%4i%02i%s", pceArg->iYear, pceArg->iMonth, (pceArg->pucDate + 6));
	    pceNew->pucSep = NULL;
	    if (ScanCalendarElementDate(pceNew) && (pndNew = xmlCopyNode(pndArg, 1)) != NULL) {
	      ScanDateIteration(pceNew);
	      InsertCalendarElementEat(pCalendarArg, pceNew, pndNew);
	    }
	    else {
	      break;
	    }
	  }
	  CalendarElementFree(pceNew);
	  xmlFreeNode(pndArg);
	}
      }
      else {
	if (pceArg->iDay < 0) {
	  /* this entry belongs to /calendar/year/month */
	  if (pCalendarArg->eType == PIE_CALENDAR_MONTH) {
	    pndParent = GetCalendarNodeMonth(pCalendarArg->pndCalendarRoot,pceArg->iYear,pceArg->iMonth);
	    if (pndParent) {
	      xmlSetProp(pndArg,BAD_CAST"idref",pceArg->pucId);
	      pndCol = FindCalendarElementCol(pndParent,pceArg->pucColId,pndArg);
	      if (pndCol) {
		xmlAddChild(pndCol,pndArg);
	      }
	      else {
		xmlFreeNode(pndArg);
	      }
	    }
	    else {
	      xmlFreeNode(pndArg);
	    }
	  }
	  else {
	    pceArg->iAnchor = GetDayAbsolute(pceArg->iYear,pceArg->iMonth,15,-1,-1); /* map to day middle of month */
	    InsertCalendarElementEat(pCalendarArg, pceArg, pndArg);
	  }
	}
	else if (pceArg->iDay == 0) {
	  /* iteration of day of month are stored at pceArg already, s. ScanDateIteration() */
	  xmlFreeNode(pndArg);
	}
	else {
	  /*  */
	  xmlFreeNode(pndArg);
	}
      }
    }
  }
}
/* End of InsertCalendarElementEat() */


/*! update calendar DOM with all registered calendar elements of 'pCalendarArg'
*/
void
CalendarUpdate(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
    /*! insert COL entities */
    pieCalendarElementPtr pceT;
    unsigned int i;

    for (pceT = pCalendarArg->pceFirst, i=0; pceT; pceT = pceT->pNext, i++) {
      xmlNodePtr pndCurrent;

      if (((IS_NODE_PIE_DATE(pceT->pndEntry)) && (pndCurrent = pceT->pndEntry->parent) != NULL)
	||
	(IS_NODE_PIE_HEADER(pceT->pndEntry) && ! IS_NODE_PIE_SECTION(pceT->pndEntry->parent) && (pndCurrent = pceT->pndEntry) != NULL)
	||
	((IS_NODE_PIE_TH(pceT->pndEntry) || IS_NODE_PIE_TD(pceT->pndEntry) || IS_NODE_PIE_TR(pceT->pndEntry)) && (pndCurrent = pceT->pndEntry) != NULL)
	||
	(IS_NODE_PIE_PAR(pceT->pndEntry) && (pndCurrent = pceT->pndEntry) != NULL)) {
	xmlChar *pucHeader;
	xmlChar *pucText;
	xmlChar *pucT;
	xmlNodePtr pndNew;

	if ((pndNew = xmlCopyNode(pndCurrent, 1))) {
	  //AddAttributeTime(pndNew);

	  if ((pucT = domGetAttributePtr(pndNew, BAD_CAST"id"))) {
	    /* rename id attribute to idref in copy of pndNew
	    */
	    xmlSetProp(pndNew, BAD_CAST"idref", pucT);
	    xmlUnsetProp(pndNew, BAD_CAST"id");
	  }

	  /*! add ancestor axis to pndNew */
	  if ((pucHeader = pieGetParentHeaderStr(pndCurrent))) {
	    domSetPropEat(pndNew, BAD_CAST "hstr", pucHeader);
	  }

	  /* inherit ancestor state attributes, if required
	  */
	  if (domGetAttributePtr(pndCurrent, BAD_CAST"state") != NULL) {
	    /* attribute exists already */
	  }
	  else if (pndCurrent->parent == NULL) {
	  }
	  else if ((pucT = domGetAttributePtr(pndCurrent->parent, BAD_CAST"state")) != NULL) {
	    xmlSetProp(pndNew, BAD_CAST"state", pucT);
	  }

	  /* inherit ancestor class attributes, if required
	  */
	  if (domGetAttributePtr(pndCurrent, BAD_CAST"class") != NULL) {
	    /* attribute exists already */
	  }
	  else if (pndCurrent->parent == NULL) {
	  }
	  else if ((pucT = domGetAttributePtr(pndCurrent->parent, BAD_CAST"class")) != NULL) {
	    xmlSetProp(pndNew, BAD_CAST"class", pucT);
	  }
	  else {
	    /* add a generic class attribute
	    */
	    assert(STR_IS_NOT_EMPTY(pndCurrent->parent->name));
	    xmlSetProp(pndNew, BAD_CAST "class", pndCurrent->parent->name);
	  }

	  /* inherit ancestor impact attributes, if required
	  */
	  if (domGetAttributePtr(pndCurrent, BAD_CAST"impact") != NULL) {
	    /* attribute exists already */
	  }
	  else if (pndCurrent->parent == NULL) {
	  }
	  else if ((pucT = domGetAttributePtr(pndCurrent->parent, BAD_CAST"impact")) != NULL) {
	    xmlSetProp(pndNew, BAD_CAST"impact", pucT);
	  }
	  else if (pndCurrent->parent->parent == NULL) {
	  }
	  else if ((pucT = domGetAttributePtr(pndCurrent->parent->parent, BAD_CAST"impact")) != NULL) {
	    xmlSetProp(pndNew, BAD_CAST"impact", pucT);
	  }
	  
	  /* check if this element has a holiday markup '+' set attribute 'holiday' to 'yes'
	  */
	  if (xmlNodeIsText(pndNew->children) && (pucT = pndNew->children->content) != NULL && pucT[0] == (xmlChar)'+') {
	    xmlSetProp(pndNew, BAD_CAST"holiday", BAD_CAST"yes");
	    for ( ; *pucT == (xmlChar)'+'; pucT++) ; /* skip markup chars */
	    for ( ; isspace(*pucT); pucT++) ;	     /* skip spaces */
	    if (STR_IS_NOT_EMPTY(pucT)) {
	      xmlChar *pucRelease = pndNew->children->content;
	      pndNew->children->content = xmlStrdup(pucT);
	      xmlFree(pucRelease);
	    }
	  }

	  InsertCalendarElementEat(pCalendarArg, pceT, pndNew);
	}
      }
#if 0
      else if (IS_NODE_PIE_SECTION(pndCurrent)) {
	xmlNodePtr pndNew;
	xmlChar *pucAttrValue;

	pndNew = xmlNewNode(NULL,NAME_PIE_SECTION);
	if (pndNew) {
	  xmlNodePtr pndHeader;

	  pndHeader = domGetFirstChild(pndCurrent,NAME_PIE_HEADER);
	  if (pndHeader) {
	    xmlNodePtr pndHeaderNew;

	    pndHeaderNew = xmlCopyNode(pndHeader,1);
	    if (pndHeaderNew) {
	      xmlAddChild(pndNew,pndHeaderNew);

	      if (pceT->patAttr->name && pceT->patAttr->children && pceT->patAttr->children->content) {	      
		xmlSetProp(pndNew,pceT->patAttr->name,pceT->patAttr->children->content);
	      }

	      pucAttrValue = domGetAttributePtr(pndCurrent,BAD_CAST "assignee");
	      if (pucAttrValue) {
		xmlSetProp(pndNew,BAD_CAST"assignee",pucAttrValue);
	      }

	      /*! add a string representation of ancestor axis to pndNew */
	      if ((pucHeader = pieGetParentHeaderStr(pndCurrent))) {
		domSetPropEat(pndNew, BAD_CAST "hstr", pucHeader);
	      }

	      pucAttrValue = domGetAttributePtr(pndCurrent,BAD_CAST "pid");
	      if (pucAttrValue) {
		xmlSetProp(pndNew,BAD_CAST"pidref",pucAttrValue);
	      }

	      InsertCalendarElementEat(pCalendarArg, pceT, pndNew);
	    }
	  }
	}
      }
      else if (IS_NODE(pndCurrent,BAD_CAST"node")) {

	if ((pucText = domGetAttributePtr(pndCurrent,BAD_CAST"TEXT"))
	  && xmlStrlen(pucText) > 0) {
	  xmlChar *pucDate = NULL;
	  xmlChar *pucDone = NULL;
	  xmlChar *pucLink;
	  xmlNodePtr pndNew;

	  pndNew = xmlNewNode(NULL,NAME_PIE_PAR);
	  if (pndNew) {
	    if ((pucLink = domGetAttributePtr(pndCurrent,BAD_CAST"LINK"))
	      && xmlStrlen(pucLink) > 0) {
	      xmlNodePtr pndLink = xmlNewChild(pndNew,NULL,NAME_PIE_LINK,pucText);
	      xmlSetProp(pndLink,BAD_CAST"href",pucLink);
	    }
	    else {
	      xmlNodeSetContent(pndNew,pucText);
	    }

	    if ((pucHeader = pieGetParentHeaderStr(pndCurrent))) {
	      domSetPropEat(pndNew, BAD_CAST "hstr", pucHeader);
	    }

	    if (pucDate) {
	      xmlSetProp(pndNew,BAD_CAST"date",pucDate);
	    }
	    if (pucDone) {
	      xmlSetProp(pndNew,BAD_CAST"class",BAD_CAST"done");
	      xmlSetProp(pndNew,BAD_CAST"done",pucDone);
	    }

	    InsertCalendarElementEat(pCalendarArg, pceT, pndNew);
	  }
	}
      }
#endif
      else if (IS_NODE_PIE_FILE(pceT->pndEntry) && (pndCurrent = pceT->pndEntry) != NULL) {
	xmlChar *pucText;

	if ((pucText = domGetAttributePtr(pndCurrent,BAD_CAST"name")) != NULL && xmlStrlen(pucText) > 0) {
	  xmlNodePtr pndNew;

	  pndNew = xmlNewNode(NULL,NAME_PIE_PAR);
	  if (pndNew) {
	    xmlChar *pucHeader;

	    xmlNewTextChild(pndNew,NULL,NAME_PIE_DATE,pceT->pucDate);
	    xmlAddChild(pndNew, xmlNewText(pucText));
	    if ((pucHeader = pieGetParentHeaderStr(pndCurrent))) {
	      domSetPropEat(pndNew, BAD_CAST "hstr", pucHeader);
	    }
	    InsertCalendarElementEat(pCalendarArg, pceT, pndNew);
	  }
	}
      }
    }
  }
} /* End of CalendarUpdate() */


/*! set attribute today at calendar DOM of 'pCalendarArg'
*/
void
CalendarSetToday(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
    xmlNodePtr pndToday = NULL;
    
    if (pCalendarArg->eType == PIE_CALENDAR_YEAR) {      
      pndToday = GetCalendarNodeYear(xmlDocGetRootElement(pCalendarArg->pdocCalendar),GetTodayYear());
    }
    else if (pCalendarArg->eType == PIE_CALENDAR_MONTH) {      
      pndToday = GetCalendarNodeMonth(xmlDocGetRootElement(pCalendarArg->pdocCalendar),GetTodayYear(),GetTodayMonth());
    }
    else if (pCalendarArg->eType == PIE_CALENDAR_WEEK) {      
      pndToday = GetCalendarNodeWeek(xmlDocGetRootElement(pCalendarArg->pdocCalendar),GetTodayYear(),GetTodayWeek());
    }
    else {      
      pndToday = GetCalendarNodeDay(xmlDocGetRootElement(pCalendarArg->pdocCalendar),GetTodayYear(),GetTodayMonth(),GetTodayDayOfMonth());
    }
    
    if (pndToday) {
      xmlSetProp(pndToday, BAD_CAST "today", BAD_CAST "yes");
    }
  }
}
/* End of CalendarSetToday() */


/*! insert a day diff attribute to all registered calendar elements of the according DOM.
*/
void
AddAttributeDayDiff(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
    /*! insert COL entities */
    pieCalendarElementPtr pceT;
    unsigned int i;

    for (pceT = pCalendarArg->pceFirst, i=0; pceT; pceT = pceT->pNext, i++) {
      xmlNodePtr pndCurrent = pceT->pndEntry;

      if (IS_VALID_NODE(pndCurrent)
	&& (IS_NODE_PIE_SECTION(pndCurrent)
	|| IS_NODE_PIE_TASK(pndCurrent)
	|| IS_NODE_PIE_DATE(pndCurrent)
	|| IS_NODE_PIE_PAR(pndCurrent))) {

	long int iDayAbsoluteMax;
	xmlChar mpucT[BUFFER_LENGTH];

	for (iDayAbsoluteMax = 0, pceT->pucSep = NULL; ScanCalendarElementDate(pceT);) {
	  long int iDayAbsolute = 0;

#ifdef DEBUG
	  PrintCalendarElement(pceT);
#endif

	  if (pceT->iMonth > 0) {
	    if (pceT->iDay > 0) {
	      iDayAbsolute = GetDayAbsolute(pceT->iYear, pceT->iMonth, pceT->iDay, -1, -1);
	    }
	    else {
	      iDayAbsolute = GetDayAbsolute(pceT->iYear, pceT->iMonth, 15, -1, -1); /* middle of the month */
	    }
	  }
	  else if (pceT->iWeek > 0) {
	    if (pceT->iDayWeek > 0) {
	      iDayAbsolute = GetDayAbsolute(pceT->iYear, -1, -1, pceT->iWeek, pceT->iDayWeek);
	    }
	    else {
	      iDayAbsolute = GetDayAbsolute(pceT->iYear, -1, -1, pceT->iWeek, 3); /* middle of the week */
	    }
	  }
	  else {
	  }
	  if (iDayAbsolute > 0 && (iDayAbsolute - GetToday()) > (iDayAbsoluteMax - GetToday())) {
	    iDayAbsoluteMax = iDayAbsolute;
	  }
	}

	xmlStrPrintf(mpucT, BUFFER_LENGTH, "%li", iDayAbsoluteMax - GetToday());
	xmlSetProp(pndCurrent, BAD_CAST"diff", mpucT);
      }
    }
  }
}
/* End of AddAttributeDayDiff() */


/*! \return an array of year numbers

syntax of \@YEAR

"2007"
"2005..2007"

*/
void
GetYearArray(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
    int *result = NULL;
    xmlChar *pucYFirst;
    xmlChar *pucYear = domGetAttributePtr(pCalendarArg->pndCalendarRoot,BAD_CAST "year");

    if (pucYear != NULL) {
      if ((pucYFirst = BAD_CAST xmlStrstr(pucYear, BAD_CAST "..")) != NULL) {
	/*
	string at pucYear contains a year range
	*/
	int y_0, yFirst;
	y_0 = atoi((const char *)pucYear);
	if (errno != ERANGE && y_0 > 1900) {
	  yFirst = atoi((const char *)(pucYFirst + 2));
	  if (errno != ERANGE) {
	    if (y_0 <= yFirst || (yFirst - y_0) < 100) {
	      int i;
	      result = (int *)xmlMemMalloc((yFirst - y_0 + 2) * sizeof(int));
	      for (i=0; y_0 + i <= yFirst; i++) {
		result[i] = y_0 + i;
	      }
	      result[i] = 0;
	    }
	  }
	}
      }
      else if (xmlStrstr(pucYear,BAD_CAST " ") != NULL || xmlStrstr(pucYear,BAD_CAST ",") != NULL) {
	/*
	string at pucYear contains a list of years
	*/
	const int i_max = 10;
	int i;

	result = (int *)xmlMemMalloc((i_max + 1) * sizeof(int));
	for (i=0; i<i_max && *pucYear != '\0'; i++) {
	  int y_0;

	  y_0 = (int)strtol((const char *)pucYear,(char **)&pucYear,10);
	  if (errno != ERANGE && y_0 > 1900) {
	    result[i] = y_0;
	  }
	  if (*pucYear == ',') {
	    pucYear++;
	  }
	}
	result[i] = 0;
      }
      else {
	/*
	string at pucYear contains a single year
	*/
	int y_0;
	y_0 = atoi((const char *)pucYear);
	if (errno != ERANGE && y_0 > 1900) {
	  result = (int *)xmlMemMalloc(2 * sizeof(int));
	  result[0] = y_0;
	  result[1] = 0;
	}
      }
    }

    if (result == NULL) {
      /*! detect the calendar years from content */
      int year_min = 2999;
      int year_max = 0;
      int i;

      GetYearMinMax(pCalendarArg,&year_min,&year_max);

      if (year_min == 2999) {
	/* no defined year found */
	year_min = GetTodayYear();
      }
      if (year_max == 0) {
	/* no defined year found */
	year_max = GetTodayYear();
      }
      /*!\todo check year_max-year_min < 10 */

      result = (int *)xmlMemMalloc((year_max-year_min+2)*sizeof(int));
      for (i=0; year_min+i <= year_max; i++) {
	result[i] = year_min + i;
      }
      result[i] = 0;
    }

    pCalendarArg->pmiYear = result;
  }
}
/* end of GetYearArray() */


/*! analyze the registered date attributes and split date sequences into separate calendar elements
 */
pieCalendarElementPtr
SplitDateSequences(pieCalendarElementPtr pceArg)
{
  pieCalendarElementPtr pceResult = NULL;

  if (pceArg != NULL && STR_IS_NOT_EMPTY(pceArg->pucDate)) {
    unsigned int i;
    xmlChar *pucNext;
    xmlChar *pucSep;

    if (pceArg->pucId == NULL) {
      /* generate an ID from pointer
      */
      pceArg->pucId = BAD_CAST xmlMalloc(65);
      if (pceArg->pndEntry) {
	xmlStrPrintf(pceArg->pucId, 64, "%p", (void *)pceArg->pndEntry);
	xmlSetProp(pceArg->pndEntry, BAD_CAST"id", pceArg->pucId);
      }
      else if (pceArg->patAttr != NULL && pceArg->patAttr->parent != NULL) {
	xmlStrPrintf(pceArg->pucId, 64, "%p", (void *)pceArg->patAttr->parent);
	xmlSetProp(pceArg->patAttr->parent, BAD_CAST"id", pceArg->pucId);
      }
    }

    pceResult = pceArg->pNext;
    pucNext = pceArg->pucDate;
    pucSep = BAD_CAST xmlStrchr(pucNext, (xmlChar)',');
    if (pucSep) {
      /* there is a trailing calendar element */
      pieCalendarElementPtr pceNew;

      pceArg->pucDate = xmlStrndup(pucNext, pucSep - pucNext);
      /* add a new calendar element into list */

      pceNew = CalendarElementDup(pceArg);
      if (pceNew) {
	pceNew->pucDate = calConcatNextDate(pucNext);
	if (pceNew->pucDate == NULL || xmlStrlen(pceNew->pucDate) < 1) {
	  CalendarElementFree(pceNew);
	  pceNew = NULL;
	}
	else {
	  pceNew->pNext = pceArg->pNext;
	  pceArg->pNext = pceNew;
	}
	pceResult = pceNew;
      }
    }
    else {
      /* a single calendar element */
      assert(xmlStrlen(pucNext) > 0);
      pceArg->pucDate = pucNext;
    }
  }

  return pceResult;
} /* end of SplitDateSequences() */


/*! register all calendar related DOM attributes
 */
BOOL_T
RegisterDateNodes(pieCalendarPtr pCalendarArg, xmlChar *pucArg)
{
  BOOL_T fResult = TRUE;

#ifdef DEBUG
  PrintFormatLog(2,"RegisterDateNodes(pCalendarArg=%0x)",pCalendarArg);
#endif

  if (pCalendarArg != NULL && pCalendarArg->pdocCalendar != NULL) {
      int i;
      xmlNodeSetPtr nodeset;
      xmlXPathObjectPtr result;
      pieCalendarElementPtr pceNew = NULL;

      //domPutDocString(stderr, BAD_CAST"Document", pCalendarArg->pdocCalendar);

      if (STR_IS_NOT_EMPTY(pucArg)) {
	result = domGetXPathNodeset(pCalendarArg->pdocCalendar, pucArg);
      }
      else {
	result = domGetXPathNodeset(pCalendarArg->pdocCalendar, BAD_CAST"/calendar/col//*[name() = 'date' or @date or @mtime2]");
      }

      if (result) {
	for (nodeset = result->nodesetval, i=0; i < nodeset->nodeNr; i++) {
	  xmlChar *pucT = NULL;

	  if (IS_NODE_PIE_DATE(nodeset->nodeTab[i])) {
	    if (IS_NODE_PIE_TD(nodeset->nodeTab[i]->parent) || IS_NODE_PIE_TH(nodeset->nodeTab[i]->parent)) {
	      pucT = domNodeGetContentPtr(nodeset->nodeTab[i]);
	    }
	    else if (nodeset->nodeTab[i]->prev == NULL && nodeset->nodeTab[i]->next == NULL) {
	      /* ignore isolated date elements */
	    }
	    else {
	      pucT = domNodeGetContentPtr(nodeset->nodeTab[i]);
	    }
	  }
	  else if ((pucT = domGetAttributePtr(nodeset->nodeTab[i], BAD_CAST "date"))) {
	  }
	  else if ((pucT = domGetAttributePtr(nodeset->nodeTab[i], BAD_CAST "done"))) {
	  }
	  else if ((pucT = domGetAttributePtr(nodeset->nodeTab[i], BAD_CAST "mtime2"))) {
	  }
	  else {
	  }

	  if (STR_IS_NOT_EMPTY(pucT) && (pceNew = CalendarElementNew(pucT)) != NULL) {
	    xmlNodePtr pndCol;

	    if (IS_NODE_PIE_DATE(nodeset->nodeTab[i])
	      && (IS_NODE_PIE_TD(nodeset->nodeTab[i]->parent) || IS_NODE_PIE_TH(nodeset->nodeTab[i]->parent))
	      && IS_NODE_PIE_TR(nodeset->nodeTab[i]->parent->parent)) {
	      pceNew->pndEntry = nodeset->nodeTab[i]->parent->parent; /* use complete table row */
	    }
	    else {
	      pceNew->pndEntry = nodeset->nodeTab[i];
	    }

	    /*! use ancestor col element for pucColId
	    */
	    for (pndCol = pceNew->pndEntry; pndCol; pndCol = pndCol->parent) {
	      if (IS_NODE_PIE_COL(pndCol)) {
		pceNew->pucColId = domGetAttributePtr(pndCol, BAD_CAST "id");
		break;
	      }
	    }

	    if (pCalendarArg->pceFirst) {
	      CalendarElementListAdd(pCalendarArg->pceFirst, pceNew);
	    }
	    else {
	      pCalendarArg->pceFirst = pceNew;
	    }
	  }
	}
	xmlXPathFreeObject(result);
      }

#ifdef DEBUG
    PrintCalendarSetup(pCalendarArg,NULL);
#endif
    fResult = TRUE;
  }

  return fResult;
} /* end of RegisterDateNodes() */


/*! 
 */
BOOL_T
ParseDates(pieCalendarPtr pCalendarArg)
{
  BOOL_T fResult = FALSE;

  if (pCalendarArg) {
    pieCalendarElementPtr pceT;

    fResult = TRUE;

    for (pceT = pCalendarArg->pceFirst; pceT; pceT = SplitDateSequences(pceT));

    for (pceT = pCalendarArg->pceFirst; pceT; pceT = pceT->pNext) {
      if (ScanCalendarElementDate(pceT)) {
	ScanDateIteration(pceT);
      }
#if 0
      else {
	//domAddNodeToError(pCalendarArg->pdocCalendar,xmlCopyNode(pceT->patAttr->parent,1));
	fResult = FALSE;
      }
#endif
    }
#ifdef DEBUG
    PrintCalendarSetup(pCalendarArg, NULL);
#endif
  }
  return fResult;
} /* end of ParseDates() */


#ifdef DEBUG
/*! process all column definition elements of /calendar/meta/calendar/ and insert result at /calendar/
 */
BOOL_T
PrintCalendarSetup(pieCalendarPtr pCalendarArg, cxpContextPtr pccArg)
{
  BOOL_T fResult = TRUE;

  if (pCalendarArg) {
    pieCalendarElementPtr pceT;
    unsigned int i;

    PrintFormatLog(1, "Calendar");
      for (pceT = pCalendarArg->pceFirst, i=0; pceT != NULL; pceT = pceT->pNext, i++) {
      assert((pceT->patAttr != NULL && pceT->patAttr->parent != NULL) || (pceT->pndEntry != NULL && pceT->pndEntry->parent != NULL));
      PrintCalendarElement(pceT);
    }
  }
  return fResult;
} /* end of PrintCalendarSetup() */
#endif


/*! process all column definition elements of /calendar/meta/calendar/ and insert result at /calendar/
 */
BOOL_T
ProcessCalendarColumns(pieCalendarPtr pCalendarArg, cxpContextPtr pccArg)
{
  BOOL_T fResult = TRUE;
  xmlNodePtr pndMeta;
  xmlNodePtr pndCalendarConfiguration;

#ifdef DEBUG
  PrintFormatLog(1, "ProcessCalendarColumns(pCalendarArg=%0x)", pCalendarArg);
#endif

  if (IS_NODE_PIE_CALENDAR(pCalendarArg->pndCalendarRoot)
    && (pndMeta = domGetFirstChild(pCalendarArg->pndCalendarRoot, NAME_META)) != NULL
    && (pndCalendarConfiguration = domGetFirstChild(pndMeta, NAME_CALENDAR)) != NULL) {
    xmlNodePtr pndCol;

    /*! preprocess COL entities */
    for (pndCol = domGetFirstChild(pndCalendarConfiguration, NAME_COL);
      pndCol;
      pndCol = domGetNextNode(pndCol, NAME_COL)) {
      xmlChar *pucIdCol;
      xmlChar *pucNameCol;
      xmlNodePtr pndColchild;
      xmlNodePtr pndColchildNext;
      xmlNodePtr pndColNew;

      pndColNew = xmlNewChild(pCalendarArg->pndCalendarRoot, NULL, NAME_COL, NULL);
      pucIdCol = domGetAttributePtr(pndCol, BAD_CAST "id");
      xmlSetProp(pndColNew, BAD_CAST "id", pucIdCol);
      pucNameCol = domGetAttributePtr(pndCol, BAD_CAST "name");
      xmlSetProp(pndColNew, BAD_CAST "name", pucNameCol);

      for (pndColchild = pndCol->children; pndColchild; pndColchild = pndColchildNext) {
	pndColchildNext = pndColchild->next;
	if (IS_NODE_XML(pndColchild)) {
	  xmlDocPtr pdocInput;

	  pdocInput = cxpProcessXmlNode(pndColchild, pccArg);
	  if (pdocInput) {
	    xmlNodePtr pndRoot = xmlDocGetRootElement(pdocInput);
	    if (pndRoot) {
	      xmlNodePtr pndMetaXml;

	      pndMetaXml = domGetFirstChild(pndRoot, NAME_META);
	      if (pndMetaXml) {
		xmlUnlinkNode(pndMetaXml);
		xmlFreeNode(pndMetaXml);
	      }
	      PrintFormatLog(2, "Insert XML DOM into calendar");
	      /*!\todo avoid copy of DOM, replace instead */
#if 0
	      xmlUnlinkNode(pndRoot);
	      xmlSetTreeDoc(pndRoot, pCalendarArg->pdocCalendar);
	      xmlAddChild(pndColNew, pndRoot);
#else
	      /*!\todo use domReplaceNodeList() */
	      xmlAddChild(pndColNew,xmlCopyNode(pndRoot,1));
#endif
	    }
	    else {
	      PrintFormatLog(1, "This DOM is empty");
	    }
	    xmlFreeDoc(pdocInput);
	  }
	}
	else if (IS_NODE_PIE_PIE(pndColchild) || IS_NODE_PIE_PAR(pndColchild) || IS_NODE_PIE_HEADER(pndColchild)
	  || IS_NODE_FILE(pndColchild) || IS_NODE_DIR(pndColchild)) {
#if 0
	  xmlUnlinkNode(pndColchild);
	  xmlAddChild(pndColNew, pndColchild);
#else
	  /*!\todo use domReplaceNodeList() */
	  xmlAddChild(pndColNew, xmlCopyNode(pndColchild, 1));
#endif
	}
	else if (IS_ENODE(pndColchild)) {
	  PrintFormatLog(1, "No valid element '%s' for CALENDAR processing", pndColchild->name);
	}
      }
    }
    fResult = TRUE;
    domUnsetNs(pndMeta);
  }
  return fResult;
} /* end of ProcessCalendarColumns() */


/*! free all calendar related DOM attributes
 */
void
CalendarFree(pieCalendarPtr pCalendarArg)
{
  pieCalendarElementPtr pceT;
  pieCalendarElementPtr pceRelease;

#ifdef DEBUG
  PrintFormatLog(1,"CalendarFree(pCalendarArg=%0x)",pCalendarArg);
#endif

  for (pceT = pCalendarArg->pceFirst; pceT; ) {
    pceRelease = pceT;
    pceT = pceRelease->pNext;

    CalendarElementFree(pceRelease);
  }

  xmlMemFree(pCalendarArg->pmiYear);

  xmlFree(pCalendarArg);
}
/* end of CalendarFree() */


/*! free all calendar column content
 */
void
CalendarColumnsFree(pieCalendarPtr pCalendarArg)
{
  if (pCalendarArg != NULL && pCalendarArg->pndCalendarRoot != NULL) {
    xmlNodePtr pndCol;

    pndCol = domGetFirstChild(pCalendarArg->pndCalendarRoot,NAME_COL);
    while (pndCol) {
      xmlNodePtr pndNext = domGetNextNode(pndCol,NAME_COL);

      xmlUnlinkNode(pndCol);
      xmlFreeNode(pndCol);
      pndCol = pndNext;
    }
  }
}
/* end of CalendarColumnsFree() */


/*!\return a new allocated calendar
 */
pieCalendarPtr
CalendarNew(void)
{
  pieCalendarPtr pCalendarResult = NULL;

  pCalendarResult = (pieCalendarPtr) xmlMalloc(sizeof(pieCalendar));
  if (pCalendarResult) {
    memset(pCalendarResult,0,sizeof(pieCalendar));
    memset(pCalendarResult->mpndDay, 0, sizeof(pCalendarResult->mpndDay));
  }
  return pCalendarResult;
}
/* end of CalendarNew() */


/*!\return a new allocated calendar
 */
pieCalendarPtr
CalendarSetup(xmlNodePtr pndArg, cxpContextPtr pccArg)
{
  pieCalendarPtr pCalendarResult = NULL;

  pCalendarResult = CalendarNew();
  if (pCalendarResult) {
    /*! create DOM
     */
    xmlChar mpucT[BUFFER_LENGTH];
    xmlNodePtr pndCalendarCopy;
    xmlNodePtr pndMeta;
    xmlNodePtr pndT;
    xmlNodePtr pndTT;
    xmlNodePtr pndTimezone;
    xmlChar *pucAttrYear;
    xmlChar *pucAttrType;
    xmlChar *pucAttrCoordinate;
    xmlChar *pucAttrTimezone;

    pCalendarResult->pdocCalendar = xmlNewDoc(BAD_CAST "1.0");
    if (IS_NODE_CALENDAR(pndArg) && pndArg->doc != NULL && STR_IS_NOT_EMPTY(pndArg->doc->URL)) {
      pCalendarResult->pdocCalendar->URL = xmlStrdup(pndArg->doc->URL);
    }
    pCalendarResult->pndCalendarRoot = xmlNewDocNode(pCalendarResult->pdocCalendar, NULL, NAME_PIE_CALENDAR, NULL);
    xmlDocSetRootElement(pCalendarResult->pdocCalendar, pCalendarResult->pndCalendarRoot);

    if (IS_NODE_CALENDAR(pndArg) && (pucAttrYear = domGetAttributePtr(pndArg, BAD_CAST "year"))) {
      xmlSetProp(pCalendarResult->pndCalendarRoot, BAD_CAST "year", pucAttrYear);
    }

    if (IS_NODE_CALENDAR(pndArg) && (pucAttrType = domGetAttributePtr(pndArg, BAD_CAST "type"))) {
      if (xmlStrEqual(pucAttrType, BAD_CAST"week")) {
	pCalendarResult->eType = PIE_CALENDAR_WEEK;
      }
      else if (xmlStrEqual(pucAttrType, BAD_CAST"wday")) {
	pCalendarResult->eType = PIE_CALENDAR_WDAY;
      }
      else if (xmlStrEqual(pucAttrType, BAD_CAST"month")) {
	pCalendarResult->eType = PIE_CALENDAR_MONTH;
      }
      else if (xmlStrEqual(pucAttrType, BAD_CAST"hour")) {
	pCalendarResult->eType = PIE_CALENDAR_MDAY_HOUR;
      }
      else if (xmlStrEqual(pucAttrType, BAD_CAST"year")) {
	pCalendarResult->eType = PIE_CALENDAR_YEAR;
      }
    }
    else {
      pCalendarResult->eType = PIE_CALENDAR_MDAY;
    }

    pCalendarResult->fCoordinate = FALSE;
    if ((pucAttrCoordinate = domGetAttributePtr(pndArg, BAD_CAST "coordinate"))
      || (pucAttrCoordinate = cxpCtxtEnvGetValueByName(pccArg, BAD_CAST "CXP_COORDINATE"))) {
      xmlSetProp(pCalendarResult->pndCalendarRoot, BAD_CAST "coordinate", pucAttrCoordinate);
      if (GetPositionISO6709((const char*)pucAttrCoordinate, &(pCalendarResult->dLatitude), &(pCalendarResult->dLongitude))) {
	/* error */
      }
      else {
	PrintFormatLog(2, "Calendar Coordinate %f/%f", pCalendarResult->dLatitude, pCalendarResult->dLongitude);
	pCalendarResult->fCoordinate = TRUE;
      }
    }

    pndMeta = xmlNewChild(pCalendarResult->pndCalendarRoot, NULL, NAME_META, NULL);
    cxpInfoProgram(pndMeta, pccArg);
    if (IS_NODE_CALENDAR(pndArg)) {
      pndCalendarCopy = xmlCopyNode(pndArg, 1);
      domValidateTree(pndCalendarCopy);
    }
    else if ((pndCalendarCopy = xmlNewNode(NULL, NAME_PIE_CALENDAR)) == NULL
      || xmlSetProp(pndCalendarCopy, BAD_CAST"subst", BAD_CAST "yes") == NULL
      || (pndT = xmlNewChild(pndCalendarCopy, NULL, NAME_PIE_COL, NULL)) == NULL
      || xmlSetProp(pndT, BAD_CAST"id", BAD_CAST "legend") == NULL
      || xmlSetProp(pndT, BAD_CAST"name", BAD_CAST"Legend") == NULL
      || (pndTT = xmlNewChild(pndT, NULL, NAME_PIE_PAR, BAD_CAST"%a %d.%m. (%j)")) == NULL
      || (pndTT = xmlNewChild(pndTT, NULL, NAME_PIE_DATE, BAD_CAST"00000000")) == NULL) {
      PrintFormatLog(1, "Cannot create new calendar");
    }
    else if ((pndT = xmlNewChild(pndCalendarCopy, NULL, NAME_PIE_COL, NULL)) == NULL
      || xmlSetProp(pndT, BAD_CAST"id", BAD_CAST "content") == NULL
      || xmlSetProp(pndT, BAD_CAST"name", BAD_CAST"Content") == NULL) {
      PrintFormatLog(1, "Cannot create new calendar");
    }
    else if ((pndTT = xmlCopyNode(pndArg, 1)) != NULL) {
      domValidateTree(pndTT);
      xmlAddChild(pndT, pndTT);
    }
    xmlAddChild(pndMeta, pndCalendarCopy);

    /* Get the current time. */
    domSetPropEat(pndMeta, BAD_CAST "ctime", GetNowFormatStr(BAD_CAST "%s"));
    domSetPropEat(pndMeta, BAD_CAST "ctime2", GetDateIsoString(0));

    /* output timezone of XML */
    if ((pucAttrTimezone = domGetAttributePtr(pndArg, BAD_CAST "timezone"))) {
      pCalendarResult->iTimezone = tzGetNumber(pucAttrTimezone);
      pCalendarResult->iTimezoneOffset = tzGetOffset(pCalendarResult->iTimezone);
    }
    pndTimezone = xmlNewChild(pndMeta, NULL, BAD_CAST"timezone", tzGetName(pCalendarResult->iTimezone));
    xmlStrPrintf(mpucT, BUFFER_LENGTH, "%+.2f", (pCalendarResult->iTimezoneOffset / 60.0f));
    xmlSetProp(pndTimezone, BAD_CAST "offset", mpucT);
    xmlSetProp(pndTimezone, BAD_CAST "id", tzGetId(pCalendarResult->iTimezone));
    PrintFormatLog(2, "Calendar timezone '%s' = %sh", tzGetName(pCalendarResult->iTimezone), domGetAttributePtr(pndTimezone, BAD_CAST "offset"));
  }
  return pCalendarResult;
} /* end of CalendarSetup() */


/*! process the required calendar files
 */
xmlDocPtr
calProcessCalendarNode(xmlNodePtr pndArg, cxpContextPtr pccArg)
{
  xmlDocPtr pdocResult = NULL;

  if (IS_NODE_CALENDAR(pndArg)) {
    pieCalendarPtr pCalendarResult;

    if ((pCalendarResult = CalendarSetup(pndArg, pccArg)) != NULL
      && ProcessCalendarColumns(pCalendarResult, pccArg)
      && RegisterDateNodes(pCalendarResult, NULL)
      && ParseDates(pCalendarResult)
      && AddYears(pCalendarResult)) {
      CalendarUpdate(pCalendarResult);
      CalendarSetToday(pCalendarResult);
      if (domGetAttributeFlag(pndArg, BAD_CAST"subst", TRUE)) {
	/* do time-consuming format substitution by explicit demand only */
	SubstituteFormat(pCalendarResult->pndCalendarRoot);
      }
#if 0
      if (domGetAttributeFlag(pndArg, BAD_CAST"columns", FALSE) == FALSE) {
	CalendarColumnsFree(pCalendarResult);
      }
#endif
      pdocResult = pCalendarResult->pdocCalendar;
      pCalendarResult->pdocCalendar = NULL;
      CalendarFree(pCalendarResult);
    }
  }
  return pdocResult;
} /* end of calProcessCalendarNode() */


/*! process the required calendar files
 */
xmlDocPtr
calProcessDoc(xmlDocPtr pdocArg, cxpContextPtr pccArg)
{
  xmlDocPtr pdocResult = NULL;
  pieCalendarPtr pCalendarResult;

  if ((pCalendarResult = CalendarSetup(xmlDocGetRootElement(pdocArg), pccArg)) != NULL
    && ProcessCalendarColumns(pCalendarResult, pccArg)
    && RegisterDateNodes(pCalendarResult, NULL)
    && ParseDates(pCalendarResult)
    && AddYears(pCalendarResult)) {
    CalendarUpdate(pCalendarResult);
    CalendarSetToday(pCalendarResult);
    SubstituteFormat(pCalendarResult->pndCalendarRoot);
#if 0
    if (domGetAttributeFlag(pndMakeCalendar, BAD_CAST"columns", FALSE) == FALSE) {
      CalendarColumnsFree(pCalendarResult);
    }
#endif
    pdocResult = pCalendarResult->pdocCalendar;
    pCalendarResult->pdocCalendar = NULL;
    CalendarFree(pCalendarResult);
  }
  return pdocResult;
} /* end of calProcessDoc() */


/*! process the required calendar files
 */
xmlDocPtr
calAddAttributeDayDiff(xmlDocPtr pdocArg)
{
  xmlDocPtr pdocResult = NULL;

#ifdef DEBUG
  PrintFormatLog(1,"calAddAttributeDayDiff(pdocArg=%0x)",pdocArg);
#endif

  if (pdocArg) {
    pieCalendarPtr pCalendarResult;

    pCalendarResult = CalendarNew();
    if (pCalendarResult) {
      /*! create DOM
      */
      pCalendarResult->pdocCalendar = pdocArg;
      pCalendarResult->pndCalendarRoot = xmlDocGetRootElement(pdocArg);
      if (RegisterDateNodes(pCalendarResult, BAD_CAST"/pie//*[name() = 'date' or @date]")
	&& ParseDates(pCalendarResult)) {
	AddAttributeDayDiff(pCalendarResult);
	pdocResult = pCalendarResult->pdocCalendar;
      }
      pCalendarResult->pdocCalendar = NULL;
      CalendarFree(pCalendarResult);
    }
  }
  return pdocResult;
} /* end of calAddAttributeDayDiff() */


/*! \return 

\param 
*/
xmlNodePtr
GetCalendarNodeYear(xmlNodePtr pndArg, unsigned int uiArgYear)
{
  xmlNodePtr pndYear;
  xmlChar mpucCompare[BUFFER_LENGTH];

  xmlStrPrintf(mpucCompare,BUFFER_LENGTH, "%02i",uiArgYear);

  for (pndYear = domGetFirstChild(pndArg,NAME_PIE_YEAR); pndYear; pndYear = domGetNextNode(pndYear,NAME_PIE_YEAR)) {
    if (xmlStrcasecmp(mpucCompare,domGetAttributePtr(pndYear,BAD_CAST "ad"))==0) {
      return pndYear;
    }
  }
  return NULL;
}
/* end of GetCalendarNodeYear() */


/*! \return 

\param 
*/
xmlNodePtr
GetCalendarNodeMonth(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth)
{
  xmlNodePtr pndYear;

  pndYear = GetCalendarNodeYear(pndArg, uiArgYear);
  if (pndYear) {
    xmlNodePtr pndMonth;
    xmlChar mpucCompare[BUFFER_LENGTH];

    xmlStrPrintf(mpucCompare,BUFFER_LENGTH, "%02i",uiArgMonth);

    for (pndMonth = domGetFirstChild(pndYear,NAME_PIE_MONTH); pndMonth; pndMonth = domGetNextNode(pndMonth,NAME_PIE_MONTH)) {
      if (xmlStrEqual(mpucCompare,domGetAttributePtr(pndMonth,BAD_CAST "nr"))) {
          return pndMonth;
      }
    }
  }
  return NULL;
}
/* end of GetCalendarNodeMonth() */


/*! \return 

\param 
*/
xmlNodePtr
GetCalendarNodeWeek(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgWeek)
{
  xmlNodePtr pndYear;

  pndYear = GetCalendarNodeYear(pndArg, uiArgYear);
  if (pndYear) {
    xmlNodePtr pndWeek;
    xmlChar mpucCompare[BUFFER_LENGTH];

    xmlStrPrintf(mpucCompare,BUFFER_LENGTH, "%02i",uiArgWeek);

    for (pndWeek = domGetFirstChild(pndYear,BAD_CAST "week"); pndWeek; pndWeek = domGetNextNode(pndWeek,BAD_CAST "week")) {
      if (xmlStrEqual(mpucCompare,domGetAttributePtr(pndWeek,BAD_CAST "nr"))) {
	return pndWeek;
      }
    }
  }
  return NULL;
}
/* end of GetCalendarNodeWeek() */


/*! \return 

\param 
*/
xmlNodePtr
GetCalendarNodeDay(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth, unsigned int uiArgDay)
{
  xmlNodePtr pndMonth;

  pndMonth = GetCalendarNodeMonth(pndArg, uiArgYear, uiArgMonth);
  if (pndMonth) {
    xmlChar mpucCompare[BUFFER_LENGTH];
    xmlNodePtr pndDay;

    xmlStrPrintf(mpucCompare,BUFFER_LENGTH, "%02i",uiArgDay);

    for (pndDay = domGetFirstChild(pndMonth,NAME_PIE_DAY); pndDay; pndDay = domGetNextNode(pndDay,NAME_PIE_DAY)) {
      if (xmlStrEqual(mpucCompare,domGetAttributePtr(pndDay,BAD_CAST "om"))) {
	return pndDay;
      }
    }
  }
  return NULL;
}
/* end of GetCalendarNodeDay() */


/*! \return 

\param 
*/
xmlNodePtr
GetCalendarNodeDayHour(xmlNodePtr pndArg, unsigned int uiArgYear, unsigned int uiArgMonth, unsigned int uiArgDay, unsigned int uiArgHour)
{
  xmlNodePtr pndDay;

  pndDay = GetCalendarNodeDay(pndArg, uiArgYear, uiArgMonth, uiArgDay);
  if (pndDay) {
    xmlChar mpucCompare[BUFFER_LENGTH];
    xmlNodePtr pndHour;

    xmlStrPrintf(mpucCompare,BUFFER_LENGTH, "%02i",uiArgHour);

    for (pndHour = domGetFirstChild(pndDay,NAME_PIE_DAY); pndHour; pndHour = domGetNextNode(pndHour,NAME_PIE_HOUR)) {
      if (xmlStrEqual(mpucCompare,domGetAttributePtr(pndHour,BAD_CAST "om"))) {
	return pndHour;
      }
    }
  }
  return NULL;
}
/* end of GetCalendarNodeDayHour() */


/*! \return a new allocated substituted string for 'fmt', 
    placeholders replaced by according attribute values of pndContext.

\param pndContext the col parent node
\param fmt
*/
xmlChar *
SubstituteFormatStr(xmlNodePtr pndContext, xmlChar *fmt)
{
  xmlChar *puc0;
  xmlChar *puc1;
  xmlChar *pucResult = NULL;
  xmlNodePtr pndYear = NULL;
  xmlNodePtr pndMonth = NULL;
  xmlNodePtr pndWeek = NULL;
  xmlNodePtr pndDay = NULL;

  if (fmt==NULL || pndContext==NULL) {
    return NULL;
  }

  for (puc0=fmt; *puc0=='\n' || *puc0=='\r'; puc0++) {}
  if (*puc0 == '\0') {
    /* ignore empty string */
    return NULL;
  }

  for (puc1=puc0, pucResult=NULL; ; puc1++) {

    if (puc1[0] == '%' && (puc1[1] == 'Y' || puc1[1] == 'm' || puc1[1] == 'd' || puc1[1] == 'a' || puc1[1] == 'j' || puc1[1] == 'M' || puc1[1] == 'V'
			   || puc1[1] == 'S' || puc1[1] == 'B' || puc1[1] == 'D')) {
      /* maybe a format char? */
      xmlChar *pucValue = NULL;

      if (pndYear == NULL) {
	/* context nodes not yet initialized */
	if (IS_NODE_PIE_YEAR(pndContext->parent)) {
	  /* node for whole year */
	  pndDay   = NULL;
	  pndWeek  = NULL;
	  pndMonth = NULL;
	  pndYear  = pndContext->parent;
	}
	else if (IS_NODE_PIE_MONTH(pndContext->parent)) {
	  pndDay   = NULL;
	  pndWeek  = NULL;
	  pndMonth = pndContext->parent;
	  pndYear  = pndContext->parent->parent;
	}
	else if (IS_NODE_PIE_WEEK(pndContext->parent)) {
	  pndDay   = NULL;
	  pndWeek  = pndContext->parent;
	  pndMonth = NULL;
	  pndYear  = pndContext->parent->parent;
	}
	else if (IS_NODE_PIE_DAY(pndContext->parent)) {
	  pndDay   = pndContext->parent;
	  pndWeek  = NULL;
	  pndMonth = pndContext->parent->parent;
	  pndYear  = pndContext->parent->parent->parent;
	}
	else {
	}
      }

      if (puc1[1] == 'Y' || puc1[1] == 'm' || puc1[1] == 'd' || puc1[1] == 'a' || puc1[1] == 'j' || puc1[1] == 'V') {
	/* one char formats */
	if (puc1[1] == 'Y') { /* year */
	  pucValue = domGetAttributePtr(pndYear,BAD_CAST "ad");
	}
	else if (puc1[1] == 'm') { /* month of year */
	  pucValue = domGetAttributePtr(pndMonth,BAD_CAST "nr");
	}
	else if (puc1[1] == 'd') { /* day of month */
	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "om");
	}
	else if (puc1[1] == 'a') { /* day of week */
	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "own");
	}
	else if (puc1[1] == 'j') { /* day of year */
	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "oy");
	}
	else if (puc1[1] == 'V') { /* ISO week of year */
	  if ((pndWeek && (pucValue = domGetAttributePtr(pndWeek,BAD_CAST "nr")))) {
	  }
	  else {
	    pucValue = domGetAttributePtr(pndDay,BAD_CAST "cw");
	  }
	}

	if (pucValue) {
	  if (puc1 - puc0 > 0) {
	    pucResult = xmlStrncat(pucResult,puc0,puc1 - puc0);
	  }

	  if (pucResult) {
	    pucResult = xmlStrcat(pucResult,pucValue);
	  }
	  else {
	    pucResult = xmlStrdup(pucValue);
	  }

	  puc1++;
	  puc0 = puc1 + 1;
	}
      }
      else if (puc1[1] == 'M') {
	/* multiple char formats */
	if (puc1[2] == 'O' && puc1[3] == 'O' && puc1[4] == 'N') {
	  if (puc1 - puc0 > 0) {
	    pucResult = xmlStrncat(pucResult,puc0,puc1 - puc0);
	  }

	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "moon");
	  if (pucValue) {
	    xmlChar *pucMoon = GetUTF8Bytes(0x274D);
	    if (pucResult) {
	      pucResult = xmlStrcat(pucResult,pucMoon);
	      xmlFree(pucMoon);
	    }
	    else {
	      pucResult = xmlStrdup(pucMoon);
	    }
	  }
	  puc1 += 4;
	  puc0 = puc1 + 1;
	}
      }
      else if (puc1[1] == 'S') { /* sun */
	if (puc1[2] == 'R') {
	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "sunrise");
	}
	else if (puc1[2] == 'S') {
	  pucValue = domGetAttributePtr(pndDay,BAD_CAST "sunset");
	}

	if (pucValue) {
	  if (puc1 - puc0 > 0) {
	    pucResult = xmlStrncat(pucResult,puc0,puc1 - puc0);
	  }

	  if (pucResult) {
	    pucResult = xmlStrcat(pucResult,pucValue);
	  }
	  else {
	    pucResult = xmlStrdup(pucValue);
	  }

	  puc1 += 2;
	  puc0 = puc1 + 1;
	}
      }
      else if (puc1[1] == 'B') {
	if (isdigit(puc1[2]) && isdigit(puc1[3]) && isdigit(puc1[4]) && isdigit(puc1[5]) && isspace(puc1[6])) {
	  /* difference in years */
	  if (puc1 - puc0 > 0) {
	    pucResult = xmlStrncat(pucResult,puc0,puc1 - puc0);
	  }

	  pucValue = GetDiffYearsStrNew(domGetAttributePtr(pndYear,BAD_CAST "ad"),&puc1[2]);
	  if (pucResult) {
	    pucResult = xmlStrcat(pucResult,pucValue);
	    xmlFree(pucValue);
	  }
	  else {
	    pucResult = pucValue;
	  }
	  puc1 += 6;
	  puc0 = puc1 + 1;
	}
      }
      else if (puc1[1] == 'D') {
	if (isdigit(puc1[2]) && isdigit(puc1[3]) && isdigit(puc1[4]) && isdigit(puc1[5])
	  && isdigit(puc1[6]) && isdigit(puc1[7]) && isdigit(puc1[8]) && isdigit(puc1[9]) && isspace(puc1[10])) {
	  /* difference in days */
	  if (puc1 - puc0 > 0) {
	    pucResult = xmlStrncat(pucResult,puc0,puc1 - puc0);
	  }

	  pucValue = GetDiffDaysStrNew(domGetAttributePtr(pndDay,BAD_CAST "abs"),&puc1[2]);
	  if (pucResult) {
	    pucResult = xmlStrcat(pucResult,pucValue);
	    xmlFree(pucValue);
	  }
	  else {
	    pucResult = pucValue;
	  }
	  puc1 += 9;
	  puc0 = puc1 + 1;
	}
      }
      /*!\todo substitute '%N' */
#if 0
      /* non-@free day of year */
      pucT = pucResult;
      if ((pucFormating = BAD_CAST xmlStrstr(pucT,BAD_CAST "%N"))) {
	/* search for all preceeding siblings in same COL in same
	YEAR
	*/
	int iDayCount = 0;
	xmlNodePtr pndTDay;
	xmlNodePtr pndCurrent;
	xmlNodePtr pndPi;
	xmlChar *pucFree;
	xmlChar *pucHoliday;
	xmlChar *pucColId;

	pucColId = domGetAttributePtr(pndContext,BAD_CAST "name");

	if (pndDay && pndYear) {
	  for (pndTDay = domGetFirstChild(domGetFirstChild(pndYear,NAME_PIE_MONTH),NAME_PIE_DAY);
	    pndTDay;
	    pndTDay = GetCalendarNodeNext(pndTDay,+1)) {
	    /* step over all day nodes */
	    /*!\todo set pndArg for every col separately, perfomance? */
	    for (pndCurrent = domGetFirstChild(pndTDay,NAME_COL);
	      pndCurrent;
	      pndCurrent = pndCurrent->next) {
	      /* step over all col nodes */
	      for (pndPi = domGetFirstChild(pndCurrent,NAME_PIE_PAR);
		pndPi;
		pndPi = pndPi->next) {
		/* step over all p nodes */
		if (((pucFree = domGetAttributePtr(pndPi,BAD_CAST "free"))
		  && !xmlStrcasecmp(pucFree,BAD_CAST "yes")
		  && !xmlStrcasecmp(domGetAttributePtr(pndCurrent,BAD_CAST "name"),pucColId))
		  ||
		  ((pucHoliday = domGetAttributePtr(pndPi,BAD_CAST "holiday"))
		  && !xmlStrcasecmp(pucHoliday,BAD_CAST "yes"))) {
		  /* a p[@free='yes'] in a col[@id=pucColId] found */
		  /* or */
		  /* a p[@holiday='yes'] found */
		  /* dont count this day */
		  goto skip_day;
		}
	      }
	    }
	    iDayCount++;
	  skip_day:
	    if (pndTDay == pndDay) {
	      /* day node itself found */
	      break;
	    }
	  }
	}

	if (iDayCount > 0) {
	  xmlStrPrintf(mpucT,BUFFER_LENGTH, "%i",iDayCount);
	  pucResult = ReplaceStr(pucT,BAD_CAST "%N",mpucT);
	}
	xmlFree(pucT);
      }
#endif
    }
    else if (puc1[0] == '\0') {
      /* end of string */
      if (pucResult) {
	pucResult = xmlStrcat(pucResult,puc0);
      }
      else {
	pucResult = xmlStrdup(puc0);
      }
      break;
    }
  }
  return pucResult;
}
/* end of SubstituteFormatStr() */


/*! Substitutes the format strings in tree of pndArg.

\param pndArg parent node
\return TRUE
*/
BOOL_T
SubstituteFormat(xmlNodePtr pndArg)
{
  xmlNodePtr pndCol;
  xmlNodePtr pndPar;
  xmlNodePtr pndPchild;

  if (pndArg==NULL || pndArg->type!=XML_ELEMENT_NODE) {
    return FALSE;
  }
  else if (IS_NODE_PIE_META(pndArg)) {
    return FALSE;
  }
  else if (IS_NODE_PIE_COL(pndArg)) {
    /*
    all COL children nodes
    */

    if (IS_NODE_PIE_CALENDAR(pndArg->parent)) {
      /* this is the calendar definition */
      return FALSE;
    }

    pndCol = pndArg;

    for (pndPar = pndCol->children; pndPar; pndPar = pndPar->next) {
      if (IS_NODE_PIE_LINK(pndPar) || IS_NODE_PIE_PAR(pndPar)) {
	/* all P children nodes */
	for (pndPchild=pndPar->children; pndPchild; pndPchild=pndPchild->next) {
	  xmlChar *pucNew;
	  if (IS_NODE_PIE_LINK(pndPchild)) {
	    if (pndPchild->children!=NULL && pndPchild->children->type == XML_TEXT_NODE) {
	      /* substitute format in text node content of link node */
	      pucNew = SubstituteFormatStr(pndCol,domNodeGetContentPtr(pndPchild));
	      if (pucNew) {
 		PrintFormatLog(4,"Substituted '%s' to '%s'",domNodeGetContentPtr(pndPchild),pucNew);
		/*!\todo encode pucNew to UTF-8 */
		xmlNodeSetContent(pndPchild->children,pucNew);
		xmlFree(pucNew);
	      }
	    }
	  }
	  else if (pndPchild->type == XML_TEXT_NODE) {
	    pucNew = SubstituteFormatStr(pndCol,pndPchild->content);
	    if (pucNew) {
	      PrintFormatLog(4,"Substituted '%s' to '%s'",pndPchild->content,pucNew);
	      /*!\todo encode pucNew to UTF-8 */
	      xmlNodeSetContent(pndPchild,pucNew);
	      xmlFree(pucNew);
	    }
	  }
	}
      }
    }
  }
  else {
    /*!\todo other elements than p ? */
    for (pndPar = pndArg->children; pndPar; pndPar = pndPar->next) {
      SubstituteFormat(pndPar);
    }
  }
  return TRUE;
}
/* end of SubstituteFormat() */


/*! adds the leading time values of pndArg content as an attribute.

\param pndArg parent node
\return TRUE
*/
BOOL_T
AddAttributeTime(xmlNodePtr pndArg)
{

#ifdef DEBUG
  PrintFormatLog(1,"AddAttributeTime(pndArg=%0x)",pndArg);
#endif

  if (pndArg==NULL || pndArg->type!=XML_ELEMENT_NODE) {
    return FALSE;
  }
  else if (IS_NODE_PIE_META(pndArg)) {
    return FALSE;
  }
  else if (IS_NODE_PIE_PAR(pndArg)) {
    /*    */
    xmlChar *pucT;

    pucT = domNodeGetContentPtr(pndArg);
    if (pucT) {
      pieCalendarElementType ceT;
      xmlChar *pucSep;

      pucSep = ScanTimeString(pucT,&ceT);
      if (pucSep) {
	if (ceT.iHourA > -1) {
	  xmlChar buffer[BUFFER_LENGTH];

	  xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iHourA > -1) ? ceT.iHourA : 0);
	  xmlSetProp(pndArg, BAD_CAST "hour", buffer);

	  xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iMinuteA > -1) ? ceT.iMinuteA : 0);
	  xmlSetProp(pndArg, BAD_CAST "minute", buffer);

	  xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iSecondA > -1) ? ceT.iSecondA : 0);
	  xmlSetProp(pndArg, BAD_CAST "second", buffer);

	  if (ceT.iHourB > -1) {
	    xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iHourB > -1) ? ceT.iHourB : 0);
	    xmlSetProp(pndArg, BAD_CAST "hour-end", buffer);

	    xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iMinuteB > -1) ? ceT.iMinuteB : 0);
	    xmlSetProp(pndArg, BAD_CAST "minute-end", buffer);

	    xmlStrPrintf(buffer,BUFFER_LENGTH-1, "%02i", (ceT.iSecondB > -1) ? ceT.iSecondB : 0);
	    xmlSetProp(pndArg, BAD_CAST "second-end", buffer);
	  }
	  while (isspace(*pucSep)) {
	    pucSep++;
	  }
	  if (xmlStrlen(pucSep) > 0) {
	    pucT = xmlStrdup(pucSep);
	    xmlNodeSetContent(pndArg,pucT);
	    xmlFree(pucT);
	  }
	}
      }
    }
  }
  return TRUE;
}
/* end of AddAttributeTime() */


/*! creates a DOM for a whole 'year'
*/
BOOL_T
AddTreeYear(pieCalendarPtr pCalendarArg, int year)
{
  int week_current;
  struct tm t;
  struct tm tFirstFirst;
  xmlChar buffer[BUFFER_LENGTH];
  xmlChar bufferYear[BUFFER_LENGTH];
  xmlChar *pucT;
  xmlNodePtr pndYear;
  xmlNodePtr pndParent = pCalendarArg->pndCalendarRoot;
  unsigned int iDayAbsolute;
  unsigned int iDayToday = GetToday();

#ifdef DEBUG
  PrintFormatLog(1,"AddTreeYear(pCalendarArg=%0x,year=%i)",pCalendarArg,year);
#endif

  if (pCalendarArg->mpndDay[0] == NULL) {
  }

  domSetPropEat(pndParent, BAD_CAST"today", GetNowFormatStr(BAD_CAST"%Y%m%d"));

  if (year > 999) {
    year -= 1900;
  }

  t.tm_year = year; 
  t.tm_yday = 0;
  t.tm_mon  = 0;
  t.tm_mday = 0;
  t.tm_wday = 0;
  t.tm_hour = 0;
  t.tm_min  = 0;
  t.tm_sec  = 0;
  t.tm_isdst  = 0;

  memcpy(&tFirstFirst, &t, sizeof(struct tm));
  tFirstFirst.tm_mday++;
  if (mktime(&tFirstFirst) == -1 || tFirstFirst.tm_year != year) {
    PrintFormatLog(1,"Year '%i' out of range", year);
    xmlStrPrintf( buffer,BUFFER_LENGTH, "Year '%i' out of range", year +  1900);
    xmlNewChild(pndParent, NULL, NAME_PIE_ERROR, buffer);
    return FALSE;
  }
  week_current = (tFirstFirst.tm_wday > 4 || tFirstFirst.tm_wday < 1) ? 0 : 1;

  pndYear = xmlNewChild(pndParent, NULL, NAME_PIE_YEAR, NULL);
  xmlStrPrintf( buffer,BUFFER_LENGTH, "%i", year +  1900);
  xmlSetProp(pndYear, BAD_CAST "ad", buffer);
  xmlStrPrintf( bufferYear,BUFFER_LENGTH, " %s ", buffer);

  if (t.tm_year > -1 && t.tm_mon > -1 && t.tm_mday > -1) {
    t.tm_mday++;
    iDayAbsolute = (unsigned int)GetDayAbsolute(t.tm_year + 1900,t.tm_mon + 1,t.tm_mday,-1,-1);

    if (iDayAbsolute < 1) {
      xmlNodePtr pndLog;
      xmlNodePtr pndText;

      pndLog = xmlNewNode(NULL,NAME_PIE_PAR);
      pndText = xmlNewText(BAD_CAST"Year out of range");
      xmlAddChild(pndLog,pndText);
      domAddNodeToError(pCalendarArg->pdocCalendar,pndLog);
    }
    else {
      xmlNodePtr pndMonth = NULL;
      xmlNodePtr pndWeek = NULL;
      xmlNodePtr pndDay;
      int iDaysDiff;
      int i;

      const char *mpucNumber[] = {
	  "00","01","02","03","04","05","06","07","08","09",
	  "10","11","12","13","14","15","16","17","18","19",
	  "20","21","22","23","24","25","26","27","28","29",
	  "30","31","32","33","34","35","36","37","38","39",
	  "40","41","42","43","44","45","46","47","48","49",
	  "50","51","52","53","54","55","56","57","58","59",
	  "60",NULL};
      
      for( ; mktime(&t) != -1 && t.tm_year == year; iDayAbsolute++, t.tm_mday++) {

	if (pCalendarArg->eType == PIE_CALENDAR_MDAY || pCalendarArg->eType == PIE_CALENDAR_MDAY_HOUR || pCalendarArg->eType == PIE_CALENDAR_MONTH) {
	  if (t.tm_mday == 1) {
	    pndMonth = xmlNewChild(pndYear, NULL, NAME_PIE_MONTH, NULL);
	    xmlSetProp(pndMonth, BAD_CAST "nr", BAD_CAST mpucNumber[t.tm_mon + 1]);
	    xmlSetProp(pndMonth, BAD_CAST "name", moy[t.tm_mon]);
	    xmlAddChild(pndMonth, xmlNewComment(bufferYear));
	  }
	}
	else if (pCalendarArg->eType == PIE_CALENDAR_WDAY || pCalendarArg->eType == PIE_CALENDAR_WDAY_HOUR || pCalendarArg->eType == PIE_CALENDAR_WEEK) {
	  if (pndWeek == NULL || t.tm_wday == 1) { /* New week on Monday */
	    pndWeek = xmlNewChild(pndYear, NULL, NAME_PIE_WEEK, NULL);
	    xmlSetProp(pndWeek, BAD_CAST "nr", BAD_CAST mpucNumber[week_current]);
	  }
	}

	if (pCalendarArg->eType == PIE_CALENDAR_MDAY || pCalendarArg->eType == PIE_CALENDAR_MDAY_HOUR
	    || pCalendarArg->eType == PIE_CALENDAR_WDAY || pCalendarArg->eType == PIE_CALENDAR_WDAY_HOUR) {

	  if (pCalendarArg->eType == PIE_CALENDAR_MDAY || pCalendarArg->eType == PIE_CALENDAR_MDAY_HOUR) {
	    pndDay = xmlNewChild(pndMonth, NULL, NAME_PIE_DAY, NULL);
	  }
	  else {
	    pndDay = xmlNewChild(pndWeek, NULL, NAME_PIE_DAY, NULL);
	  }

	  xmlSetProp(pndDay, BAD_CAST "mon", BAD_CAST mpucNumber[t.tm_mon + 1]);

	  xmlSetProp(pndDay, BAD_CAST "om", BAD_CAST mpucNumber[t.tm_mday]);

	  xmlStrPrintf( buffer,BUFFER_LENGTH, "%i", t.tm_yday + 1);
	  xmlSetProp(pndDay, BAD_CAST "oy", buffer);

	  xmlStrPrintf( buffer,BUFFER_LENGTH, "%i", t.tm_wday);
	  xmlSetProp(pndDay, BAD_CAST "ow", BAD_CAST mpucNumber[t.tm_wday]);
	  /* attribute is used in calGetWeekNode() */
	  xmlSetProp(pndDay, BAD_CAST "own", dow[t.tm_wday]);

	  xmlSetProp(pndDay, BAD_CAST "cw", BAD_CAST mpucNumber[week_current]);

	  xmlStrPrintf( buffer, BUFFER_LENGTH, "%i", iDayAbsolute);
	  xmlSetProp(pndDay, BAD_CAST "abs", buffer);

	  if (pCalendarArg->fCoordinate) {
	    double dHourUTCSunrise;
	    double dHourUTCSunset;
	    int iMinute;

	    /*\bug non-sense values  sunrise="67:20" sunset="75:07" */

	    sun_rise_set(t.tm_year + 1900,t.tm_mon + 1,t.tm_mday,
		pCalendarArg->dLongitude,pCalendarArg->dLatitude,
		&dHourUTCSunrise,&dHourUTCSunset);

	    // Sunrise
	    dHourUTCSunrise += pCalendarArg->iTimezoneOffset; // FIXME
	    if (t.tm_isdst) {
	      dHourUTCSunrise += 1.0;
	    }
	    iMinute = RoundToInt(dHourUTCSunrise * 60.0);
	    xmlStrPrintf(buffer,BUFFER_LENGTH, "%i:%02i", iMinute / 60, iMinute % 60);
	    xmlSetProp(pndDay,BAD_CAST "sunrise",buffer);

	    // Sunset
	    dHourUTCSunset += pCalendarArg->iTimezoneOffset; // FIXME
	    if (t.tm_isdst) {
	      dHourUTCSunset += 1.0;
	    }
	    iMinute = RoundToInt(dHourUTCSunset * 60.0);
	    xmlStrPrintf(buffer,BUFFER_LENGTH, "%i:%02i", iMinute / 60, iMinute % 60);
	    xmlSetProp(pndDay,BAD_CAST "sunset",buffer);

	    // Moon
	    if (IsFullMoonConway(t.tm_year + 1900,t.tm_mon + 1,t.tm_mday)) {
	      xmlSetProp(pndDay,BAD_CAST "moon",BAD_CAST"full");
	    }

	    xmlSetProp(pndDay,BAD_CAST "dst", BAD_CAST(t.tm_isdst ? "yes" : "no"));
	  }

	  /*
	        add the time difference from today in days
	   */
	  pCalendarArg->mpndDay[iDayAbsolute] = pndDay;

	  iDaysDiff = iDayAbsolute - iDayToday;
	  xmlStrPrintf( buffer, BUFFER_LENGTH, "%i", iDaysDiff);
	  xmlSetProp(pndDay, BAD_CAST "diff", buffer);
	  
	  if (pCalendarArg->eType == PIE_CALENDAR_MDAY_HOUR || pCalendarArg->eType == PIE_CALENDAR_WDAY_HOUR) {
	    for (i=0; i<24; i++) {
	      xmlNodePtr pndHour;

	      pndHour = xmlNewChild(pndDay, NULL, NAME_PIE_HOUR, NULL);
	      xmlSetProp(pndHour, BAD_CAST "nr", BAD_CAST mpucNumber[i]);
	    }
	  }
	}
	else if (pCalendarArg->eType == PIE_CALENDAR_WEEK) {
	  pCalendarArg->mpndDay[iDayAbsolute] = pndWeek;
	}
	else if (pCalendarArg->eType == PIE_CALENDAR_MONTH) {
	  pCalendarArg->mpndDay[iDayAbsolute] = pndMonth;
	}
	else if (pCalendarArg->eType == PIE_CALENDAR_YEAR) {
	  pCalendarArg->mpndDay[iDayAbsolute] = pndYear;
	}

	if (t.tm_wday == 0) { /* am Sonntag */
	  week_current++;
	}
      }
      return TRUE;
    }
  }
  return FALSE;
}
/* end of AddTreeYear() */


/*! This is based on a 'do it in your head' algorithm by John Conway. In its current form, it's only valid for the 20th and 21st centuries, but I'm sure John's got refinements. :)

http://www.faqs.org/faqs/astronomy/faq/part3/section-15.html
http://www.ben-daglish.net/moon.shtml

  \return 1 in case of error
 */
BOOL_T
IsFullMoonConway(int year, int month, int day)
{
  int r;
  double d;

  r = year % 100;
  r %= 19;
  if (r>9) {
    r -= 19;
  }
  r = ((r * 11) % 30) + month + day;
  if (month<3) {
    r += 2;
  }
  d =  r - ((year<2000) ? 4.0 : 8.3);

  r = (int)floor(d + 0.5) % 30;
  if (r < 0) {
    r += 30;
  }

  return (r == 15);
}
/* end of IsFullMoonConway() */


#ifdef TESTCODE
#include "test/test_pie_calendar.c"
#endif

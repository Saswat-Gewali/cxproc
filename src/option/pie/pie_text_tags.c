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


/*
 */
#include <libxml/tree.h>

 /*
  */
#include "basics.h"
#include "utils.h"
#ifdef HAVE_PETRINET
#include <cxp/cxp.h>
#include <petrinet/petrinet.h>
#else
#include "dom.h"
#endif
#include <pie/pie_dtd.h>
#include <pie/pie_text_tags.h>


static pcre2_code* re_tag = NULL;

/* multibyte characters */
#define UTF8_UMLAUT "\xC3\xA4" "\xC3\x84" "\xC3\xB6" "\xC3\x96" "\xC3\xBC" "\xC3\x9C" "\xC3\x9F"

#if 1
#define RE_TAG "\\b([A-Z][A-Z0-9]{3,}|[A-Za-z" UTF8_UMLAUT "]{9,})\\b"
#else
#define RE_TAG "\\b([A-Z]{3,}|[A-Z]{1,4}[a-z" UTF8_UMLAUT "]{3,}|[0-9A-Za-z" UTF8_UMLAUT "]{6,})\\b"
#endif

/* core regexp for hashtags, can be extended by a XML processing instruction 'tag-regexp' */
#define RE_HASHTAG "[#@][A-Za-z0-9_" UTF8_UMLAUT "]+"

/* default extension of the hashtag regexp if no PI can be found */
#define RE_HASHTAG_EXT "[A-Z]{3,}|[A-Z]{1,4}[a-z" UTF8_UMLAUT "]{5,}|[0-9A-Za-z" UTF8_UMLAUT "]{8,}"


static xmlNodePtr
AddTagNodeNew(xmlNodePtr pndArg, const xmlChar* pucArg);

static BOOL_T
_TagNodeIsCovered(xmlNodePtr pndArgA, xmlNodePtr pndArgB);

static xmlNodePtr
SplitStringToHashTagNodes(const xmlChar* pucArg, pcre2_code* preArg);

static xmlChar*
StringUpdateMarkupNew(xmlChar* pucArg, int* piArg);

static BOOL_T
TagStrIsCovered(xmlChar* pucArgTag, xmlChar* pucArgTagNeedle);

static xmlNodePtr
AppendListTag(xmlNodePtr pndTags, xmlChar* pucArg);

static BOOL_T
CleanListTag(xmlNodePtr pndArg);

static xmlNodePtr
RecognizeGlobalTags(xmlNodePtr pndTags, xmlNodePtr pndArg);

static BOOL_T
RecognizeNodeTags(xmlNodePtr pndTags, xmlNodePtr pndArg, pcre2_code* preArg);


/*! exit procedure for this module
*/
void
pieTextTagsCleanup(void)
{
  if (re_tag) {
    pcre2_code_free(re_tag);
    re_tag = NULL;
  }
} /* end of pieTextTagsCleanup() */


/*! update legacy markup in pucArg

\param pucArg
\return updated copy of pucArg or NULL in case of errors
*/
xmlChar*
StringUpdateMarkupNew(xmlChar* pucArg, int* piArg)
{
  xmlChar* pucResult = NULL;

  if (pucArg) {
    int i, j, k;
    xmlChar* pucT;

    /* try to find backwards an 'ending' separator */
    for (j=xmlStrlen(pucArg), k=0; j>0; j--) {
      if (pucArg[j]==(xmlChar)'|') {
	/* count separators backwards */
	k++;
      }
      else if (k > 0) {
	if (isspace(pucArg[j])) {
	}
	else {
	  k = 0; /* to be ignored */
	}
	break;
      }
    }

    if (k > 0) { /* there is more than one separator and a space char before */

      j++;

      pucResult = xmlStrndup(pucArg, j);

      for (i=j+k; pucArg[i]; ) {
	int l;

	for (; isspace(pucArg[i]); i++);

	for (j=i; ishashtag(&pucArg[i], &l); i += l);

	if (i > j) { /* there are hashtag chars */

	  if (pucArg[j] == (xmlChar)'#' || pucArg[j] == (xmlChar)'@') {
	    /* there is hashtag markup already */
	  }
	  else {
	    pucResult = xmlStrcat(pucResult, BAD_CAST" @");
	    pucResult = xmlStrncat(pucResult, &pucArg[j], i-j);
#if 1
	    if ((pucT = BAD_CAST StringEndsWith((char*)pucResult, "_org")) != NULL
	      || (pucT = BAD_CAST StringEndsWith((char*)pucResult, "_do")) != NULL
	      || (pucT = BAD_CAST StringEndsWith((char*)pucResult, "_review")) != NULL
	      || (pucT = BAD_CAST StringEndsWith((char*)pucResult, "_rejected")) != NULL) {
	      /* split GTD categories '_org', '_do', '_review', '_rejected' */
	      pucT[0] = (xmlChar)'#';
	    }
#endif
	  }
	}
	else if (isend(pucArg[i])) { /* neither space nor hashtag chars */
	  break;
	}
	else {
	  i++;
	}
      }

      if (piArg) {
	*piArg = (k > 2) ? 1 : ((k > 1) ? 2 : 0);
      }
    }
    else {
      pucResult = xmlStrdup(pucArg);
    }

    if ((pucT = BAD_CAST StringEndsWith((char*)pucResult, "+++")) != NULL) {
      if (piArg) *piArg = 1;
      pucT[0] = (xmlChar)'\0';
    }
    else if ((pucT = BAD_CAST StringEndsWith((char*)pucResult, "++")) != NULL) {
      if (piArg) *piArg = 2;
      pucT[0] = (xmlChar)'\0';
    }
  }
  return pucResult;
} /* end of StringUpdateMarkupNew() */


/*! add a new 'tag' element to pndArg if not yet exists
*/
xmlNodePtr
AddTagNodeNew(xmlNodePtr pndArg, const xmlChar* pucArg)
{
  xmlNodePtr pndResult = NULL;

  if (pndArg != NULL && STR_IS_NOT_EMPTY(pucArg)) {
    xmlNodePtr pndT;
    xmlChar* pucT;

    /* find an existing htag node with same content */
    for (pndT = pndArg->children; pndT; pndT = pndT->next) {
      if (IS_NODE_PIE_TTAG(pndT) && xmlStrEqual(pucArg, domNodeGetContentPtr(pndT))) {
	/* it's there already */
	break;
      }
    }

    if (pndT == NULL) {
      pndResult = xmlNewTextChild(pndArg, NULL, NAME_PIE_TTAG, pucArg);
    }
    else {
      domIncrProp(pndT, BAD_CAST"count", 1);
    }

#if 0
    /*! check if there are German Umlaute in pucArg
     */
    if ((pucT = StringReplaceUmlauteNew(pucArg)) != NULL) {
      pndResult = AddTagNodeNew(pndArg, pucT);
      xmlFree(pucT);
    }
#endif
    
  }
  return pndResult;
} /* End of AddTagNodeNew() */


/*! splits an UTF-8 string into a list of text and 'htag' element nodes
*/
xmlNodePtr
SplitStringToHashTagNodes(const xmlChar* pucArg, pcre2_code* preArg)
{
  size_t ducOrigin;
  xmlNodePtr pndResult = NULL;

  if (pucArg != NULL && (ducOrigin = xmlStrlen(pucArg)) > 0) {
    int rc;
    pcre2_match_data* match_data;
    xmlNodePtr pndT;
    xmlNodePtr pndPostfix;

    match_data = pcre2_match_data_create_from_pattern(preArg, NULL);
    rc = pcre2_match(
      preArg,        /* result of pcre2_compile() */
      (PCRE2_SPTR8)pucArg,  /* the subject string */
      xmlStrlen(pucArg),             /* the length of the subject string */
      0,              /* start at offset 0 in the subject */
      0,              /* default options */
      match_data,        /* vector of integers for substring information */
      NULL);            /* number of elements (NOT size in bytes) */

    if (rc > -1) {
      PCRE2_SIZE* ovector;

      ovector = pcre2_get_ovector_pointer(match_data);
      /*! exclude some other uses of '#' and '@' */
      if (ovector[0] > 0 && pucArg[ovector[0]-1] == (xmlChar)'&' && pucArg[ovector[0]] == (xmlChar)'#'
	&& ((pucArg[ovector[0]+1] == (xmlChar)'x' && ishexdigit(pucArg[ovector[0]+2])) || isdigit(pucArg[ovector[0]+1]))) {
	/* XML numeric character reference '&#x1F3A;' or '&#1234;' */
      }
      else if (ovector[1] - ovector[0] > 2) {
	/*
	the regexp match, assemble node list with a common dummy
	element node
	*/
	xmlChar* pucHashtag;
	xmlChar* pucA = (xmlChar*)pucArg + ovector[0];

	pucHashtag = xmlStrndup(pucA, ovector[1] - ovector[0]);
	PrintFormatLog(3, "Hashtag '%s' (%i..%i) in '%s'", pucHashtag, ovector[0], ovector[1], pucArg);

	pndResult = xmlNewNode(NULL, BAD_CAST "dummy");

	if (ovector[0] > 0) {
	  /* the content starts with text	*/
	  xmlChar* pucT = xmlStrndup(pucArg, ovector[0]);
	  xmlAddChild(pndResult, xmlNewText(pucT));
	  xmlFree(pucT);
	}

	if ((pndT = AddTagNodeNew(pndResult, pucHashtag))) {
	  xmlNodeSetName(pndT, NAME_PIE_HTAG);
	}

	if (ducOrigin > ovector[1]) {
	  /* the content ends with text, recursion */
	  pndPostfix = SplitStringToHashTagNodes(pucArg + ovector[1], preArg);
	  if (pndPostfix) {
	    pndT = pndPostfix->children;
	    domUnlinkNodeList(pndT);
	    xmlAddChildList(pndResult, pndT);
	    xmlFreeNode(pndPostfix);
	  }
	  else {
	    xmlChar* pucT = xmlStrdup(pucArg + ovector[1]);
	    xmlAddChild(pndResult, xmlNewText(pucT));
	    xmlFree(pucT);
	  }
	}

	xmlFree(pucHashtag);
      }
    }

    pcre2_match_data_free(match_data);   /* Release memory used for the match */
  }
  return pndResult;
} /* end of SplitStringToHashTagNodes() */


/*! find all explicit Hashtags '#' and '@'

http://microformats.org/wiki/twitter-syntax
*/
BOOL_T
RecognizeHashtags(xmlNodePtr pndArg, pcre2_code* preArg)
{
  BOOL_T fResult = TRUE;
  xmlChar* pucREHashTag = NULL;
  xmlNodePtr pndChild;
  xmlNodePtr pndChildPI;
  int errornumber = 0;
  size_t erroroffset;

  if (IS_NODE_META(pndArg) || IS_NODE_PIE_PRE(pndArg) || IS_NODE_PIE_DATE(pndArg)) {
    /* skip */
  }
  else if (IS_NODE_PIE_ETAG(pndArg) || IS_NODE_PIE_HTAG(pndArg) || IS_NODE_PIE_TTAG(pndArg)) {
    /* skip existing tag elements */
  }
  else if (IS_NODE_PIE_BLOCK(pndArg)
	   &&
	   (((pndChildPI = pndArg->children) != NULL && pndChildPI->type == XML_PI_NODE) || ((pndChildPI = pndArg->last) != NULL && pndChildPI->type == XML_PI_NODE))
	   &&
	   xmlStrEqual(pndChildPI->name,BAD_CAST"tag-regexp")
	   &&
	   ((pucREHashTag = domNodeGetContentPtr(pndChildPI)) != NULL && xmlStrlen(pucREHashTag) > 0)
	   ) {
    /* there is a local regexp string for tags */
    xmlChar* pucT = NULL;
    pcre2_code* preBlock = NULL;

    /*!\todo check string to avoid regular expression injection */

    pucT = xmlStrdup(BAD_CAST RE_HASHTAG);
    pucT = xmlStrcat(pucT, BAD_CAST"|");
    pucT = xmlStrcat(pucT, pucREHashTag);

    PrintFormatLog(2, "Initialize hashtag regexp '%s'", pucT);
    preBlock = pcre2_compile(
			     (PCRE2_SPTR8)pucT, /* the pattern */
			     PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
			     PCRE2_UTF|PCRE2_CASELESS,        /* default options */
			     &errornumber,          /* for error number */
			     &erroroffset,          /* for error offset */
			     NULL);                 /* use default compile context */

    if (preBlock) {
      for (pndChild = pndArg->children; fResult && pndChild != NULL; pndChild = pndChild->next) {
	fResult = RecognizeHashtags(pndChild, preBlock);
      }
      pcre2_code_free(preBlock);
    }
    else {
      /* regexp error handling */
      PrintFormatLog(1, "hashtag regexp '%s' error: '%i'", pucT, errornumber);
      fResult = FALSE;
    }
    xmlFree(pucT);
  }
  else if (preArg == NULL) {
    /* build default regexp */
    PrintFormatLog(2, "Initialize default hashtag regexp '%s'", RE_HASHTAG "|" RE_HASHTAG_EXT);
    preArg = pcre2_compile(
#ifdef EXPERIMENTAL
			     (PCRE2_SPTR8)RE_HASHTAG "|" RE_HASHTAG_EXT, /* the pattern */
#else
			     (PCRE2_SPTR8)RE_HASHTAG, /* the pattern */
#endif			     
			     PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
			     PCRE2_UTF,        /* default options */
			     &errornumber,          /* for error number */
			     &erroroffset,          /* for error offset */
			     NULL);                 /* use default compile context */

    if (preArg) {
      for (pndChild = pndArg->children; fResult && pndChild != NULL; pndChild = pndChild->next) {
	fResult = RecognizeHashtags(pndChild, preArg);
      }
      pcre2_code_free(preArg);
    }
    else {
      /* regexp error handling */
      PrintFormatLog(1, "hashtag regexp '%s' error: '%i'", RE_HASHTAG, errornumber);
      fResult = FALSE;
    }
  }
  else if (IS_ENODE(pndArg) && (pndArg->ns==NULL)) { //  || pndArg->ns==pnsPie

    for (pndChild = pndArg->children;
      fResult && pndChild != NULL;
      pndChild = (pndChild != NULL) ? pndChild->next : NULL) {
      int iWeight = 0;
      xmlChar* pucT;

      if (xmlNodeIsText(pndChild) && (pucT = StringUpdateMarkupNew(pndChild->content, &iWeight)) != NULL) { /* pndChild is a text node */
	xmlNodePtr pndReplace;

	pndReplace = SplitStringToHashTagNodes(pucT, preArg);
	if (pndReplace) {
	  /* there is a result list */
	  xmlNodePtr pndT;

	  pndT = pndChild->next;
	  if (domReplaceNodeList(pndChild, pndReplace->children) == pndChild) {
	    xmlFreeNodeList(pndChild);
	  }
	  xmlFreeNode(pndReplace);
	  /*  */
	  if (pndT != NULL && pndT->prev != NULL) {
	    pndChild = pndT->prev;
	  }
	  else {
	    pndChild = NULL;
	  }
	}
	else {
	  xmlNodeSetContent(pndChild,pucT); /* set updated string as content */
	}

	if (iWeight > 0 && iWeight < 3) {
	  if (IS_NODE_PIE_LINK(pndArg)) {
	    xmlSetProp(pndArg->parent, BAD_CAST"impact", BAD_CAST((iWeight < 2) ? "1" : "2"));
	  }
	  else {
	    xmlSetProp(pndArg, BAD_CAST"impact", BAD_CAST((iWeight < 2) ? "1" : "2"));
	  }
	}

	xmlFree(pucT);
      }
      else {
	fResult = RecognizeHashtags(pndChild, preArg);
      }
    }
  }
  return fResult;
} /* End of RecognizeHashtags() */


/*! inherit the impact attribute and htag nodes from an ancestor node
*/
BOOL_T
InheritHashtags(xmlDocPtr pdocArg)
{
  BOOL_T fResult = FALSE;

  if (pdocArg != NULL) {
    xmlNodePtr pndRoot;

    if ((pndRoot = xmlDocGetRootElement(pdocArg))) {
      xmlNodePtr pndPieXPathRoot;
      xmlXPathObjectPtr result;

      if ((result = domGetXPathNodeset(pdocArg, BAD_CAST"//*[name() = 'task' or name() = 'p' or name() = 'h' or name() = 'link' or name() = 'th' or name() = 'td']")) != NULL) {
	int i;
	xmlNodeSetPtr nodeset;

	nodeset = result->nodesetval;
	if (nodeset->nodeNr > 0) {
	  for (i=0; i < nodeset->nodeNr; i++) {
	    xmlChar* pucT;
	    xmlNodePtr pndT;
#if 0
	    if (domGetAttributePtr(nodeset->nodeTab[i], BAD_CAST"impact") == NULL) {
	      /* try to find this attribute at a ancestor node */
	      for (pndT=nodeset->nodeTab[i]->parent; pndT; pndT=pndT->parent) {
		if ((pucT = domGetAttributePtr(pndT, BAD_CAST"impact"))) {
		  xmlSetProp(nodeset->nodeTab[i], BAD_CAST"impact", pucT);
		  break;
		}
	      }
	    }
#endif
	    if (IS_NODE_PIE_HEADER(nodeset->nodeTab[i]) || IS_NODE_PIE_LINK(nodeset->nodeTab[i]) || IS_NODE_PIE_TH(nodeset->nodeTab[i]) || IS_NODE_PIE_TD(nodeset->nodeTab[i])) {
	      for (pndT=nodeset->nodeTab[i]->children; pndT; pndT=pndT->next) {
		if (IS_NODE_PIE_HTAG(pndT) && pndT->children != NULL) {
		  AddTagNodeNew(nodeset->nodeTab[i]->parent, pndT->children->content);
		}
	      }
	    }
	    else if (IS_NODE_PIE_PAR(nodeset->nodeTab[i])) {
	      if (IS_NODE_PIE_LIST(nodeset->nodeTab[i]->parent)) {
		/* append this tags to parent element 'list' */
		for (pndT=nodeset->nodeTab[i]->children; pndT; pndT=pndT->next) {
		  if (IS_NODE_PIE_HTAG(pndT) && pndT->children != NULL) {
		    AddTagNodeNew(nodeset->nodeTab[i]->parent, pndT->children->content);
		  }
		}
	      }
	      else {
		for (pndT=nodeset->nodeTab[i]->children; pndT; pndT=pndT->next) {
		  if (IS_NODE_PIE_HTAG(pndT) && pndT->children != NULL) {
		    AddTagNodeNew(nodeset->nodeTab[i], pndT->children->content);
		  }
		}
	      }
	    }
	  }
	  fResult = TRUE;
	}
	xmlXPathFreeObject(result);
      }
    }
  }
  return fResult;
} /* end of InheritHashtags() */


/*!
*/
int
_TagMatch(xmlChar* pucArgA, xmlChar* pucArgB)
{
  int iResult = -1;

  assert(pucArgA != pucArgB);
  assert(STR_IS_NOT_EMPTY(pucArgA));
  assert(STR_IS_NOT_EMPTY(pucArgB));

  for (iResult=0; ; iResult++) {
    if (pucArgA[iResult] == '\0' || pucArgB[iResult] == '\0') {
      break; /* end of a string found */
    }
    else if (pucArgB[iResult] == pucArgA[iResult]) {
    }
    else {
      break; /* end of match found */
    }
  }
  return iResult;
} /* end of TagMatch() */


/*! clean list of tags and count (cleanup of similar tags)

\todo compress list, use attribute weight to remove redundant tags

\return pointer to first node of list
*/
BOOL_T
CleanListTag(xmlNodePtr pndArg)
{
  BOOL_T fResult = FALSE;

  if (IS_NODE_PIE_TAGLIST(pndArg) && IS_NODE_PIE_TTAG(pndArg->children)) {
    xmlNodePtr pndTag;
    xmlNodePtr pndTagNext;

    PrintFormatLog(3, "Cleaning Tags");

    for (pndTag = pndArg->children; pndTag; pndTag = pndTagNext) {
      xmlChar* pucTag;

      pndTagNext = pndTag->next;

      if (pndTag->children != NULL && pndTag->children->type == XML_TEXT_NODE && (pucTag = pndTag->children->content) != NULL) { /* there is text content */
	xmlNodePtr pndI;
	xmlNodePtr pndINext;

	for (pndI = pndArg->children; pndI; pndI = pndINext) {
	  xmlChar* pucI;

	  pndINext = pndI->next;

	  if (pndI != pndTag
	      && pndI->children != NULL && pndI->children->type == XML_TEXT_NODE && (pucI = pndI->children->content) != NULL
	      && ((pucI[0] != (xmlChar)'@' && pucI[0] != (xmlChar)'#') || xmlStrEqual(pucI, pucTag))
	      && StringBeginsWith((char*)pucI, (char*)pucTag)
	      // && TagMatch(pucI,pucTag) > 4
	    ) {

	    /*!
	      merge similar node pndI to pndTag
	    */

	    int iCountNeedle;
	    xmlChar* pucAttrCount;

	    if ((pucAttrCount = domGetAttributePtr(pndI, BAD_CAST"count"))) {
	      iCountNeedle = atoi((char*)pucAttrCount);
	    }
	    else {
	      iCountNeedle = 1;
	    }
	    domIncrProp(pndTag, BAD_CAST"count", iCountNeedle);

	    if (pndI == pndTagNext) {
	      pndTagNext = pndINext; /* fix pndTagNext before node will be removed */
	    }
	    xmlUnlinkNode(pndI);
	    xmlFreeNode(pndI);
	  }
	}
      }
      else {
	/*!
	  remove this empty tag
	*/
	xmlUnlinkNode(pndTag);
	xmlFreeNode(pndTag);
      }
    }
    fResult = TRUE;
  }
  return fResult;
} /* End of CleanListTag() */


/*! recognize all remarkable text tags and append them as childs to pndTags

\return pointer to first node of list
*/
xmlNodePtr
AppendListTag(xmlNodePtr pndTags, xmlChar* pucArg)
{
  xmlNodePtr pndResult = pndTags;

  if (STR_IS_NOT_EMPTY(pucArg)) {
    if (pndResult == NULL) {
      /* very first new tag element */
      pndResult = xmlNewNode(NULL, NAME_PIE_TTAG);
      if (pndResult) {
	xmlNodeSetContent(pndResult, pucArg);
      }
    }
    else if (IS_NODE_PIE_TTAG(pndResult)) {
      xmlNodePtr pndT;

      for (pndT = pndResult; pndT; pndT = pndT->next) {
	if (TagStrIsCovered(domNodeGetContentPtr(pndT), pucArg)) {
	  /* tag element exists already */
	  domIncrProp(pndT, BAD_CAST"count", 1); /*! increment count attribute */
	  break;
	}

	if (pndT->next == NULL) { /* end of node list */
	  /* new tag element */
	  xmlNodePtr pndTagNew;

	  pndTagNew = xmlNewNode(NULL, NAME_PIE_TTAG);
	  if (pndTagNew) {
	    xmlNodeSetContent(pndTagNew, pucArg);
	    pndT->next = pndTagNew;
	    pndTagNew->prev = pndT;
	  }
	  break;
	}
      }
    }
    else {
      assert(FALSE);
    }
  }
  return pndResult;
}
/* End of AppendListTag() */


/*! recognize all remarkable text tags and append them as childs to pndTags
*/
BOOL_T
RecognizeNodeTags(xmlNodePtr pndTags, xmlNodePtr pndArg, pcre2_code* preArg)
{
  BOOL_T fResult = TRUE;

  if (re_tag == NULL) {
    int errornumber = 0;
    size_t erroroffset;

    /* use default regexp */
    PrintFormatLog(2, "Initialize tag regexp with '%s'", RE_TAG);
    re_tag = pcre2_compile(
      (PCRE2_SPTR8)RE_TAG, /* the pattern */
      PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
      0,        /* default options */
      &errornumber,          /* for error number */
      &erroroffset,          /* for error offset */
      NULL);                 /* use default compile context */

    if (re_tag == NULL) {
      /* regexp error handling */
      PrintFormatLog(1, "Tag regexp '%s' error: '%i'", RE_TAG, errornumber);
      //fResult = FALSE;
    }
  }

  if (IS_NODE_META(pndArg) || IS_NODE_PIE_PRE(pndArg) || IS_NODE_PIE_LINK(pndArg) || IS_NODE_PIE_DATE(pndArg)) {
    /* skip */
  }
  else if (IS_NODE_PIE_ETAG(pndArg) || IS_NODE_PIE_HTAG(pndArg) || IS_NODE_PIE_TTAG(pndArg)) {
    /* skip existing tag elements */
  }
  else if (IS_ENODE(pndArg) && (pndArg->ns==NULL)) { //  || pndArg->ns==pnsPie
    xmlChar* pucT;
    xmlChar* pucTag;
    xmlNodePtr pndListTag = NULL;
    xmlNodePtr pndChild;

    /*!\todo add tags also if PIE/XML is the input */
#if 0
    /*! append element name value as an additional tag */
    if (IS_NODE_PIE_TASK(pndArg) || IS_NODE_PIE_FIG(pndArg)) {
      pucT = xmlStrdup(BAD_CAST"#");
      pucT = xmlStrcat(pucT, pndArg->name);
      pndListTag = AppendListTag(pndListTag, pucT);
      xmlFree(pucT);
    }

    /*! append class attribute value as an additional tag */
    pucTag = domGetAttributePtr(pndArg, BAD_CAST"class");
    if (STR_IS_NOT_EMPTY(pucTag)) {

      pucT = xmlStrdup(BAD_CAST"#");
      pucT = xmlStrcat(pucT, pucTag);
      pndListTag = AppendListTag(pndListTag, pucT);
      xmlFree(pucT);
    }
#endif

    for (pndChild = pndArg->children; pndChild; pndChild=pndChild->next) {
      xmlChar* pucSubstr;

      if (xmlNodeIsText(pndChild) && (pucSubstr = pndChild->content) && STR_IS_NOT_EMPTY(pucSubstr)) {
	int i;

	for (i=0; pucSubstr; i++) {
	  int rc;
	  pcre2_match_data* match_data;

	  match_data = pcre2_match_data_create_from_pattern(preArg, NULL);
	  rc = pcre2_match(
	    preArg,        /* result of pcre2_compile() */
	    (PCRE2_SPTR8)pucSubstr,  /* the subject string */
	    xmlStrlen(pucSubstr),             /* the length of the subject string */
	    0,              /* start at offset 0 in the subject */
	    0,              /* default options */
	    match_data,        /* vector of integers for substring information */
	    NULL);            /* number of elements (NOT size in bytes) */

	  if (rc > -1) {
	    PCRE2_SIZE* ovector;

	    ovector = pcre2_get_ovector_pointer(match_data);
	    PrintFormatLog(4, "tag %i..%i in '%s'", ovector[0], ovector[1], pucSubstr);
	    pucTag = xmlStrndup(pucSubstr + ovector[0], ovector[1] - ovector[0]);
	    pndListTag = AppendListTag(pndListTag, pucTag);
	    xmlFree(pucTag);
	    if (*(pucSubstr + ovector[1]) == '\0') {
	      /* there are no trailing chars */
	      pucSubstr = NULL;
	    }
	    else {
	      pucSubstr += ovector[1];
	    }
	  }
	  else {
	    pucSubstr = NULL;
	  }
	  pcre2_match_data_free(match_data);   /* Release memory used for the match */
	}
      }
      else if (IS_NODE_PIE_ETAG(pndChild)) {
	/* use text content of explicit tag as tag */
	if (pndChild->children != NULL && (pucTag = pndChild->children->content) != NULL) {
	  pndListTag = AppendListTag(pndListTag, pucTag);
	}
      }
      else if (IS_NODE_PIE_TR(pndChild)
	|| IS_NODE_PIE_SECTION(pndChild)
	|| IS_NODE_PIE_TASK(pndChild)
	|| IS_NODE_PIE_FIG(pndChild)
#ifdef HAVE_PETRINET
	|| IS_NODE_PKG2_STELLE(pndChild)
	|| IS_NODE_PKG2_TRANSITION(pndChild)
	|| IS_NODE_PKG2_REQUIREMENT(pndChild)
#endif
	|| IS_NODE_PIE_PAR(pndChild)) {
	fResult = RecognizeNodeTags(pndChild, pndChild, preArg); /* append tag list to this element */
      }
      else if (IS_NODE_PIE_HEADER(pndChild)) {
	fResult = RecognizeNodeTags(pndChild->parent, pndChild, preArg); /* append tag list to his parent element */
      }
      else {
	fResult = RecognizeNodeTags(pndTags, pndChild, preArg); /* append tag list to an ancestor node */
      }
    }

    if (pndListTag) {
      xmlAddChildList(pndTags, pndListTag);
    }
  }
  return fResult;
} /* End of RecognizeNodeTags() */


/*! process the

\todo optimize performance of RecognizeNodeTags(), CleanListTag() and use it
 */
xmlNodePtr
ProcessTags(xmlDocPtr pdocPie, xmlChar* pucAttrTags)
{
  xmlNodePtr pndResult = NULL;
  xmlNodePtr pndRoot;

  if (pdocPie != NULL && (pndRoot = xmlDocGetRootElement(pdocPie)) != NULL) {
    int errornumber = 0;
    size_t erroroffset;
    xmlNodePtr pndMeta;
    xmlNodePtr pndTags = NULL;

    if ((pndMeta = domGetFirstChild(pndRoot, NAME_META)) != NULL
      && (pndTags = domGetFirstChild(pndMeta, NAME_PIE_TTAG)) != NULL) {
      /* there is a tags element already */
      PrintFormatLog(2, "Remove old tags");
      xmlUnlinkNode(pndTags);
      xmlFreeNode(pndTags);
      pndTags = NULL;
    }

    RecognizeHashtags(pndRoot, NULL);
    InheritHashtags(pdocPie);
      
    if ((pndMeta = domGetFirstChild(pndRoot, NAME_META)) == NULL) {
      pndMeta = xmlNewChild(pndRoot, NULL, NAME_PIE_META, NULL);
    }
    pndTags = xmlNewChild(pndMeta, NULL, NAME_PIE_TAGLIST, NULL);

#if 0
    PrintFormatLog(3, "Recognize Tags");
    if (pucAttrTags == NULL || xmlStrlen(pucAttrTags) < 1 || xmlStrEqual(pucAttrTags, BAD_CAST"yes")) {
      RecognizeNodeTags(pndRoot, pndRoot, re_tag);
    }
    else if (xmlStrlen(pucAttrTags) > 1) {
      /* use default regexp */
      pcre2_code* re_tag_local = NULL;

      PrintFormatLog(2, "Initialize tag regexp with '%s'", pucAttrTags);
      re_tag_local = pcre2_compile(
	(PCRE2_SPTR8)pucAttrTags, /* the pattern */
	PCRE2_ZERO_TERMINATED, /* indicates pattern is zero-terminated */
	0,        /* default options */
	&errornumber,          /* for error number */
	&erroroffset,          /* for error offset */
	NULL);                 /* use default compile context */

      if (re_tag_local) {
	RecognizeNodeTags(pndRoot, pndRoot, re_tag_local);
	pcre2_code_free(re_tag_local);
      }
      else {
	/* regexp error handling */
	PrintFormatLog(1, "Tag regexp '%s' error: '%i'", RE_TAG, errornumber);
      }
    }
#endif

    RecognizeGlobalTags(pndTags, pndRoot);
    CleanListTag(pndTags);
    pndResult = pndTags;
  }

  return pndResult;
} /* End of ProcessTags() */


/*! recognize all remarkable text tags and append them as childs to pndTags
 */
xmlNodePtr
RecognizeGlobalTags(xmlNodePtr pndTags, xmlNodePtr pndArg)
{
  if (IS_NODE_META(pndArg) || IS_NODE_PIE_PRE(pndArg) || IS_NODE_PIE_LINK(pndArg)) {
    /* skip */
  }
  else if (domGetAttributeNode(pndArg,BAD_CAST"hidden") != NULL) {
    /* skip */
  }
  else if (IS_ENODE(pndArg) && (pndArg->ns==NULL)) { //  || pndArg->ns==pnsPie
    xmlNodePtr pndChild;

    for (pndChild = pndArg->children; pndChild; pndChild=pndChild->next) {
      if ((IS_NODE_PIE_TTAG(pndChild) || IS_NODE_PIE_ETAG(pndChild))
	&& xmlNodeIsText(pndChild->children) && STR_IS_NOT_EMPTY(pndChild->children->content)) {
	xmlNodePtr pndT;

	pndT = xmlCopyNode(pndChild, 1);
	if (pndT) {
	  xmlNodeSetName(pndT, NAME_PIE_TTAG);
	  xmlAddChild(pndTags, pndT);
	}
      }
      else {
	RecognizeGlobalTags(pndTags, pndChild);
      }
    }
  }
  return NULL;
}
/* End of RecognizeGlobalTags() */


/*! \return TRUE if tag pucArgB is covered by pucArgA
*/
BOOL_T
TagStrIsCovered(xmlChar* pucArgA, xmlChar* pucArgB)
{
  BOOL_T fResult = FALSE;

  if (pucArgA == pucArgB) {
    PrintFormatLog(2, "Tag is identical");
  }
  else if (STR_IS_EMPTY(pucArgA)) {
    PrintFormatLog(3, "Tag is empty");
  }
  else if (STR_IS_EMPTY(pucArgB)) {
    PrintFormatLog(3, "Tag B is empty");
    fResult = TRUE;
  }
  else if (xmlStrcasecmp(pucArgA, pucArgB) == 0) {
    PrintFormatLog(4, "Tag '%s' is equal to '%s'", pucArgA, pucArgB);
    fResult = TRUE;
  }
#if 0
  else if (pucArgA[0] == (xmlChar)'@' || pucArgA[0] == (xmlChar)'#') {
    PrintFormatLog(4, "Tag '%s' is explicit", pucArgA);
  }
  else if (pucArgB[0] == (xmlChar)'@' || pucArgB[0] == (xmlChar)'#') {
    PrintFormatLog(4, "Tag '%s' is explicit", pucArgB);
  }
  else if (StringBeginsWith((char*)pucArgB, (char*)pucArgA)) {
    PrintFormatLog(3, "Tag '%s' covers '%s'", pucArgA, pucArgB);
    fResult = TRUE;
  }
#endif
  
  return fResult;
} /* End of TagStrIsCovered() */


/*! \return TRUE if
*/
BOOL_T
_TagNodeIsCovered(xmlNodePtr pndArgA, xmlNodePtr pndArgB)
{
  return (pndArgA != pndArgB
    && IS_NODE_PIE_TTAG(pndArgA) && IS_NODE_PIE_TTAG(pndArgB)
    && xmlNodeIsText(pndArgA->children) && xmlNodeIsText(pndArgB->children)
    && TagStrIsCovered(pndArgA->children->content, pndArgB->children->content));
} /* End of TagNodeIsCovered() */


/*! \return pointer to next element in iteration if pndArgB is merged to pndArgA successfully
*/
xmlNodePtr
MergeTagNodes(xmlNodePtr pndArgA, xmlNodePtr pndArgB)
{
  xmlNodePtr pndResult = NULL;

  assert(pndArgA != pndArgB);

  if (IS_NODE_PIE_TTAG(pndArgA) && IS_NODE_PIE_TTAG(pndArgB)) {
    if (domNodeGetContentPtr(pndArgB)) {
      int iCountNeedle;
      xmlChar* pucAttrCount;

      if ((pucAttrCount = domGetAttributePtr(pndArgB, BAD_CAST"count"))) {
	iCountNeedle = atoi((char*)pucAttrCount);
      }
      else {
	iCountNeedle = 1;
      }
      domIncrProp(pndArgA, BAD_CAST"count", iCountNeedle);
    }

    pndResult = pndArgB->next;
    xmlUnlinkNode(pndArgB);
    xmlFreeNode(pndArgB);
  }
  return pndResult;
} /* End of MergeTagNodes() */



#ifdef TESTCODE
#include "test/test_pie_text_tags.c"
#endif

/* -*- Mode: C++; c-default-style: "k&r"; indent-tabs-mode: nil; tab-width: 2; c-basic-offset: 2 -*- */

/* libstaroffice
* Version: MPL 2.0 / LGPLv2+
*
* The contents of this file are subject to the Mozilla Public License Version
* 2.0 (the "License"); you may not use this file except in compliance with
* the License or as specified alternatively below. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
*
* Software distributed under the License is distributed on an "AS IS" basis,
* WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
* for the specific language governing rights and limitations under the
* License.
*
* Major Contributor(s):
* Copyright (C) 2002 William Lachance (wrlach@gmail.com)
* Copyright (C) 2002,2004 Marc Maurer (uwog@uwog.net)
* Copyright (C) 2004-2006 Fridrich Strba (fridrich.strba@bluewin.ch)
* Copyright (C) 2006, 2007 Andrew Ziem
* Copyright (C) 2011, 2012 Alonso Laurent (alonso@loria.fr)
*
*
* All Rights Reserved.
*
* For minor contributions see the git repository.
*
* Alternatively, the contents of this file may be used under the terms of
* the GNU Lesser General Public License Version 2 or later (the "LGPLv2+"),
* in which case the provisions of the LGPLv2+ are applicable
* instead of those above.
*/

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include <librevenge/librevenge.h>

#include "SDWParser.hxx"
#include "SWFieldManager.hxx"
#include "SWFormatManager.hxx"
#include "StarDocument.hxx"
#include "StarFileManager.hxx"
#include "StarZone.hxx"

#include "StarAttribute.hxx"

/** Internal: the structures of a StarAttribute */
namespace StarAttributeInternal
{
////////////////////////////////////////
//! Internal: the state of a StarAttribute
struct State {
  //! constructor
  State()
  {
  }
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
StarAttribute::StarAttribute() : m_state(new StarAttributeInternal::State)
{
}

StarAttribute::~StarAttribute()
{
}

bool StarAttribute::readAttribute(StarZone &zone, int nWhich, int nVers, long lastPos, StarDocument &document)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  libstoff::DebugStream f;
  f << "Entries(StarAttribute)[" << zone.getRecordLevel() << "]:";

  long pos=input->tell();
  int val;
  switch (nWhich) {
  case ATTR_CHR_CASEMAP:
    f << "chrAtrCaseMap=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_CHARSETCOLOR: {
    f << "chrAtrCharSetColor=" << input->readULong(1) << ",";
    STOFFColor color;
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find a color\n"));
      f << "###aColor,";
      break;
    }
    if (!color.isBlack())
      f << color << ",";
    break;
  }
  case ATTR_CHR_COLOR: {
    f << "chrAtrColor,";
    STOFFColor color;
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find a color\n"));
      f << "###aColor,";
      break;
    }
    if (!color.isBlack())
      f << color << ",";
    break;
  }
  case ATTR_CHR_CONTOUR:
    f << "chrAtrContour=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_CROSSEDOUT:
    f << "chrAtrCrossedOut=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_ESCAPEMENT:
    f << "chrAtrEscapement=" << input->readULong(1) << ",";
    f << "nEsc=" << input->readLong(2) << ",";
    break;
  case ATTR_CHR_FONT:
  case ATTR_CHR_CJK_FONT:
  case ATTR_CHR_CTL_FONT: {
    if (nWhich==ATTR_CHR_FONT)
      f << "chrAtrFont,";
    else if (nWhich==ATTR_CHR_CJK_FONT)
      f << "chrAtrCJKFont,";
    else
      f << "chrAtrCTLFont,";
    f << "family=" << input->readULong(1) << ",";
    f << "ePitch=" << input->readULong(1) << ",";
    int encoding=(int) input->readULong(1);
    f << "eTextEncoding=" << encoding << ",";
    librevenge::RVNGString fName, string;
    if (!zone.readString(fName)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the name\n"));
      f << "###aName,";
      break;
    }
    if (!fName.empty())
      f << "aName=" << fName.cstr() << ",";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the style\n"));
      f << "###aStyle,";
      break;
    }
    if (!string.empty())
      f << "aStyle=" << string.cstr() << ",";
    if (encoding!=10 && fName=="StarBats" && input->tell()<lastPos) {
      if (input->readULong(4)==0xFE331188) {
        // reread data in unicode
        if (!zone.readString(fName)) {
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the name\n"));
          f << "###aName,";
          break;
        }
        if (!fName.empty())
          f << "aNameUni=" << fName.cstr() << ",";
        if (!zone.readString(string)) {
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the style\n"));
          f << "###aStyle,";
          break;
        }
        if (!string.empty())
          f << "aStyleUni=" << string.cstr() << ",";
      }
      else input->seek(-3, librevenge::RVNG_SEEK_CUR);
    }
    break;
  }
  case ATTR_CHR_FONTSIZE:
  case ATTR_CHR_CJK_FONTSIZE:
  case ATTR_CHR_CTL_FONTSIZE:
    if (nWhich==ATTR_CHR_FONTSIZE)
      f << "chrAtrFontSize,";
    else if (nWhich==ATTR_CHR_CJK_FONTSIZE)
      f << "chrAtrCJKFontSize,";
    else
      f << "chrAtrCTLFontSize,";
    f << "size=" << input->readULong(2) << ",";
    f << "nProp=" << input->readULong((nVers>=1) ? 2 : 1) << ",";
    if (nVers>=2) f << "nPropUnit=" << input->readULong(2) << ",";
    break;
  case ATTR_CHR_KERNING:
    f << "chrAtrKerning=" << input->readULong(2) << ",";
    break;
  case ATTR_CHR_LANGUAGE:
  case ATTR_CHR_CJK_LANGUAGE:
  case ATTR_CHR_CTL_LANGUAGE:
  case ATTR_SC_LANGUAGE_FORMAT:
    f << (nWhich==ATTR_CHR_LANGUAGE ? "chrAtrLanguage": nWhich==ATTR_CHR_CJK_LANGUAGE ? "chrAtrCJKLanguage" :
          nWhich==ATTR_CHR_CTL_LANGUAGE ? "chrAtrCTLLanguage" : "scLanguage");
    f << "=" << input->readULong(2) << ",";
    break;
  case ATTR_CHR_POSTURE:
  case ATTR_CHR_CJK_POSTURE:
  case ATTR_CHR_CTL_POSTURE:
    f << (nWhich==ATTR_CHR_POSTURE ? "chrAtrPosture": nWhich==ATTR_CHR_CJK_POSTURE ? "chrAtrCJKPosture" : "chrAtrCTLPosture");
    f << "=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_PROPORTIONALFONTSIZE:
    f << "chrAtrProportionFontSize,";
    f << "size=" << input->readULong(2) << ",";
    break;
  case ATTR_CHR_SHADOWED:
    f << "chrAtrShadowed=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_UNDERLINE:
    f << "chrAtrUnderline=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_WEIGHT:
  case ATTR_CHR_CJK_WEIGHT:
  case ATTR_CHR_CTL_WEIGHT:
    f << (nWhich==ATTR_CHR_WEIGHT ? "chrAtrWeight" : nWhich==ATTR_CHR_CJK_WEIGHT ? "chrAtrCJKWeight" : "chrAtrCTLWeight");
    f << "=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_WORDLINEMODE:
    f << "chrAtrWordlineMode=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_AUTOKERN:
    f << "chrAtrAutoKern=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_BLINK:
    f << "chrAtrBlink=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_NOHYPHEN:
    f << "chrAtrNoHyphen=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_NOLINEBREAK:
    f << "chrAtrNoLineBreak=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_BACKGROUND:
  case ATTR_FRM_BACKGROUND: {
    f << (nWhich==ATTR_CHR_BACKGROUND ? "chrAtrBackground" : "background") << "=" << input->readULong(1) << ",";
    STOFFColor color;
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read color\n"));
      f << "###color,";
      break;
    }
    else if (!color.isWhite())
      f << "color=" << color << ",";
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read fill color\n"));
      f << "###fillcolor,";
      break;
    }
    else if (!color.isWhite())
      f << "fillcolor=" << color << ",";
    f << "nStyle=" << input->readULong(1) << ",";
    if (nVers<1) break;
    int doLoad=(int) input->readULong(2);
    if (doLoad & 1) { // TODO: svx_frmitems.cxx:1948
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: do not know how to load graphic\n"));
      f << "###graphic,";
      break;
    }
    if (doLoad & 2) {
      librevenge::RVNGString link;
      if (!zone.readString(link)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the link\n"));
        f << "###link,";
        break;
      }
      if (!link.empty())
        f << "link=" << link.cstr() << ",";
    }
    if (doLoad & 4) {
      librevenge::RVNGString filter;
      if (!zone.readString(filter)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the filter\n"));
        f << "###filter,";
        break;
      }
      if (!filter.empty())
        f << "filter=" << filter.cstr() << ",";
    }
    f << "nPos=" << input->readULong(1) << ",";
    break;
  }
  case ATTR_CHR_ROTATE:
    f << "chrAtrRotate,";
    f << "nVal=" << input->readULong(2) << ",";
    f << "b=" << input->readULong(1) << ",";
    break;
  case ATTR_CHR_EMPHASIS_MARK:
    f << "chrAtrEmphasisMark=" << input->readULong(2) << ",";
    break;
  case ATTR_CHR_TWO_LINES: { // checkme
    f << "chrAtrTwoLines=" << input->readULong(1) << ",";
    f << "nStart[unicode]=" << input->readULong(2) << ",";
    f << "nEnd[unicode]=" << input->readULong(2) << ",";
    break;
  }
  case ATTR_CHR_SCALEW:
    f << "chrAtrScaleW=" << input->readULong(2);
    if (input->tell()<lastPos) {
      f << "nVal=" << input->readULong(2) << ",";
      f << "test=" << input->readULong(2) << ",";
    }
    break;
  case ATTR_CHR_RELIEF:
    f << "chrAtrRelief=" << input->readULong(2);
    break;
  case ATTR_CHR_DUMMY1:
    f << "chrAtrDummy1=" << input->readULong(1) << ",";
    break;

  // text attribute
  case ATTR_TXT_INETFMT: {
    f << "textAtrInetFmt,";
    // SwFmtINetFmt::Create
    librevenge::RVNGString string;
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find string\n"));
      f << "###url,";
      break;
    }
    if (!string.empty())
      f << "url=" << string.cstr() << ",";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find string\n"));
      f << "###targert,";
      break;
    }
    if (!string.empty())
      f << "target=" << string.cstr() << ",";
    f << "nId1=" << input->readULong(2) << ",";
    f << "nId2=" << input->readULong(2) << ",";
    int nCnt=(int) input->readULong(2);
    bool ok=true;
    f << "libMac=[";
    for (int i=0; i<nCnt; ++i) {
      if (input->tell()>lastPos) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a libmac name\n"));
        f << "###libname,";
        ok=false;
        break;
      }
      if (!zone.readString(string)) {
        f << "###string,";
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a string\n"));
        ok=false;
        break;
      }
      else if (!string.empty())
        f << string.cstr() << ":";
      if (!zone.readString(string)) {
        f << "###string,";
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a string\n"));
        ok=false;
        break;
      }
      else if (!string.empty())
        f << string.cstr();
      f << ",";
    }
    f << "],";
    if (!ok) break;
    if (nVers>=1) {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find string\n"));
        f << "###aName1,";
        break;
      }
      if (!string.empty())
        f << "aName1=" << string.cstr() << ",";
    }
    if (nVers>=2) {
      nCnt=(int) input->readULong(2);
      f << "libMac2=[";
      for (int i=0; i<nCnt; ++i) {
        f << "nCurKey=" << input->readULong(2) << ",";
        if (input->tell()>lastPos) {
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a libmac name\n"));
          f << "###libname2,";
          break;
        }
        if (!zone.readString(string)) {
          f << "###string,";
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a string\n"));
          break;
        }
        else if (!string.empty())
          f << string.cstr() << ":";
        if (!zone.readString(string)) {
          f << "###string,";
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a string\n"));
          break;
        }
        else if (!string.empty())
          f << string.cstr();
        f << "nScriptType=" << input->readULong(2) << ",";
      }
      f << "],";
    }
    break;
  }
  case ATTR_TXT_DUMMY4:
    f << "textAtrDummy4=" << input->readULong(1) << ",";
    break;
  case ATTR_TXT_REFMARK: {
    f << "textAtrRefMark,";
    librevenge::RVNGString string;
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find aName\n"));
      f << "###aName,";
      break;
    }
    if (!string.empty())
      f << "aName=" << string.cstr() << ",";
    break;
  }
  case ATTR_TXT_TOXMARK: {
    f << "textAtrToXMark,";
    int cType=(int) input->readULong(1);
    f << "cType=" << cType << ",";
    f << "nLevel=" << input->readULong(2) << ",";
    librevenge::RVNGString string;
    int nStringId=0xFFFF;
    if (nVers<1) {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find aTypeName\n"));
        f << "###aTypeName,";
        break;
      }
      if (!string.empty())
        f << "aTypeName=" << string.cstr() << ",";
    }
    else {
      nStringId=(int) input->readULong(2);
      if (nStringId!=0xFFFF)
        f << "nStringId=" << nStringId << ",";
    }
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find aAltText\n"));
      f << "###aAltText,";
      break;
    }
    if (!string.empty())
      f << "aAltText=" << string.cstr() << ",";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find aPrimKey\n"));
      f << "###aPrimKey,";
      break;
    }
    if (!string.empty())
      f << "aPrimKey=" << string.cstr() << ",";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find aSecKey\n"));
      f << "###aSecKey,";
      break;
    }
    if (!string.empty())
      f << "aSecKey=" << string.cstr() << ",";
    if (nVers>=2) {
      cType=(int) input->readULong(1);
      f << "cType=" << cType << ",";
      nStringId=(int) input->readULong(2);
      if (nStringId!=0xFFFF)
        f << "nStringId=" << nStringId << ",";
      f << "cFlags=" << input->readULong(1) << ",";
    }
    if (nVers>=1 && nStringId!=0xFFFF) {
      if (!zone.getPoolName(nStringId, string)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find a nId name\n"));
        f << "###nId=" << nStringId << ",";
      }
      else
        f << string.cstr() << ",";
    }
    break;
  }
  case ATTR_TXT_CHARFMT:
    f << "textAtrCharFmt=" << input->readULong(2) << ",";
    break;
  case ATTR_TXT_DUMMY5:
    f << "textAtrDummy5=" << input->readULong(1) << ",";
    break;
  case ATTR_TXT_CJK_RUBY:
    f << "textAtrCJKRuby=" << input->readULong(1) << ",";
    break;
  case ATTR_TXT_UNKNOWN_CONTAINER:
  case ATTR_SC_USERDEF:
    // call SfxPoolItem::Create which does nothing
    f << (nWhich==ATTR_TXT_UNKNOWN_CONTAINER ?  "textAtrUnknownContainer" : "scUserDef") << ",";
    break;
  case ATTR_TXT_DUMMY6:
    f << "textAtrDummy6" << input->readULong(1) << ",";
    break;
  case ATTR_TXT_DUMMY7:
    f << "textAtrDummy7" << input->readULong(1) << ",";
    break;

  // field...
  case ATTR_TXT_FIELD: {
    f << "textAtrField,";
    SWFieldManager fieldManager;
    fieldManager.readField(zone);
    break;
  }
  case ATTR_TXT_FLYCNT: {
    f << "textAtrFlycnt,";
    SWFormatManager formatManager;
    if (input->peek()=='o')
      formatManager.readSWFormatDef(zone,'o', document);
    else
      formatManager.readSWFormatDef(zone,'l', document);
    break;
  }
  case ATTR_TXT_FTN: {
    f << "textAtrFtn,";
    // sw_sw3npool.cxx SwFmtFtn::Create
    f << "number1=" << input->readULong(2) << ",";
    librevenge::RVNGString string;
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the aNumber\n"));
      f << "###aNumber,";
      break;
    }
    if (!string.empty())
      f << "aNumber=" << string.cstr() << ",";
    // no sure, find this attribute once with a content here, so ...
    if (!document.getSDWParser()->readSWContent(zone, document)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the content\n"));
      f << "###aContent,";
      break;
    }
    if (nVers>=1) {
      uint16_t nSeqNo;
      *input >> nSeqNo;
      if (nSeqNo) f << "nSeqNo=" << nSeqNo << ",";
    }
    if (nVers>=1) {
      uint8_t nFlags;
      *input >> nFlags;
      if (nFlags) f << "nFlags=" << nFlags << ",";
    }
    break;
  }
  case ATTR_TXT_SOFTHYPH: // ok no data
    f << "textAtrSoftHyph,";
    break;
  case ATTR_TXT_HARDBLANK: // ok no data
    f << "textAtrHardBlank,";
    break;
  case ATTR_TXT_DUMMY1:
    f << "textAtrDummy1" << input->readULong(1) << ",";
    break;
  case ATTR_TXT_DUMMY2:
    f << "textAtrDummy2" << input->readULong(1) << ",";
    break;

  // paragraph attribute
  case ATTR_PARA_LINESPACING:
    f << "parAtrLineSpacing,";
    f << "nPropSpace=" << input->readLong(1) << ",";
    f << "nInterSpace=" << input->readLong(2) << ",";
    f << "nHeight=" << input->readULong(2) << ",";
    f << "nRule=" << input->readULong(1) << ",";
    f << "nInterRule=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_ADJUST:
    f << "parAtrAdjust=" << input->readULong(1) << ",";
    if (nVers>=1) f << "nFlags=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_SPLIT:
    f << "parAtrSplit=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_ORPHANS:
    f << "parAtrOrphans,";
    f << "nLines="  << input->readLong(1) << ",";
    break;
  case ATTR_PARA_WIDOWS:
    f << "parAtrWidows,";
    f << "nLines="  << input->readLong(1) << ",";
    break;
  case ATTR_PARA_TABSTOP: {
    f << "parAtrTabStop,";
    int N=(int) input->readULong(1);
    if (input->tell()+7*N>lastPos) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: N is too big\n"));
      f << "###N=" << N << ",";
      N=int(lastPos-input->tell())/7;
    }
    f << "tabs=[";
    for (int i=0; i<N; ++i) {
      int nPos=(int) input->readLong(4);
      f << nPos << "->" << input->readLong(1) << ":" << input->readLong(1) << ":" << input->readLong(1) << ",";
    }
    f << "],";
    break;
  }
  case ATTR_PARA_HYPHENZONE:
    f << "parAtrHyphenZone=" << input->readLong(1) << ",";
    f << "bHyphenPageEnd=" << input->readLong(1) << ",";
    f << "nMinLead=" << input->readLong(1) << ",";
    f << "nMinTail=" << input->readLong(1) << ",";
    f << "nMaxHyphen=" << input->readLong(1) << ",";
    break;
  case ATTR_PARA_DROP:
    f << "parAtrDrop,";
    f << "nFmt=" << input->readULong(2) << ",";
    f << "nLines1=" << input->readULong(2) << ",";
    f << "nChars1=" << input->readULong(2) << ",";
    f << "nDistance1=" << input->readULong(2) << ",";
    if (nVers>=1)
      f << "bWhole=" << input->readULong(1) << ",";
    else {
      f << "nX=" << input->readULong(2) << ",";
      f << "nY=" << input->readULong(2) << ",";
    }
    break;
  case ATTR_PARA_REGISTER:
    f << "parAtrRegister=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_NUMRULE: {
    f << "parAtrNumRule,";
    librevenge::RVNGString string;
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the sTmp\n"));
      f << "###sTmp,";
      break;
    }
    if (!string.empty())
      f << "sTmp=" << string.cstr() << ",";
    if (nVers>0)
      f << "nPoolId=" << input->readULong(2) << ",";
    break;
  }
  case ATTR_PARA_SCRIPTSPACE:
    f << "parAtrScriptSpace=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_HANGINGPUNCTUATION:
    f << "parAtrHangingPunctuation=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_FORBIDDEN_RULES:
    f << "parAtrForbiddenRules=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_VERTALIGN:
    f << "parAtrVertAlign=" << input->readULong(2) << ",";
    break;
  case ATTR_PARA_SNAPTOGRID:
    f << "parAtrSnapToGrid=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_CONNECT_BORDER:
    f << "parAtrConnectBorder=" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_DUMMY5:
    f << "parAtrDummy5" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_DUMMY6:
    f << "parAtrDummy6" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_DUMMY7:
    f << "parAtrDummy7" << input->readULong(1) << ",";
    break;
  case ATTR_PARA_DUMMY8:
    f << "parAtrDummy8" << input->readULong(1) << ",";
    break;

  // frame parameter
  case ATTR_FRM_FILL_ORDER:
    f << "fillOrder=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_FRM_SIZE:
    f << "frmSize,";
    f << "sizeType=" << input->readULong(1) << ",";
    f << "width=" << input->readULong(4) << ",";
    f << "height=" << input->readULong(4) << ",";
    if (nVers>1)
      f << "percent=" << input->readULong(1) << "x"  << input->readULong(1) << ",";
    break;
  case ATTR_FRM_PAPER_BIN:
    f << "paperBin=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_LR_SPACE:
    f << "lrSpace,";
    f << "left=" << input->readULong(2);
    val=(int) input->readULong(nVers>=1 ? 2 : 1);
    if (val) f << ":" << val;
    f << ",";
    f << "right=" << input->readULong(2);
    val=(int) input->readULong(nVers>=1 ? 2 : 1);
    if (val) f << ":" << val;
    f << ",";
    f << "firstLine=" << input->readLong(2);
    val=(int) input->readULong(nVers>=1 ? 2 : 1);
    if (val) f << ":" << val;
    f << ",";
    if (nVers>=2)
      f << "textLeft=" << input->readLong(2) << ",";
    if (nVers>=3) {
      long marker=(long) input->readULong(4);
      if (marker==0x599401FE)
        f << "firstLine[bullet]=" << input->readULong(2);
      else if (input->tell()==lastPos+3) // normally end by 0
        input->seek(-3, librevenge::RVNG_SEEK_CUR);
      else
        input->seek(-4, librevenge::RVNG_SEEK_CUR);
    }
    break;
  case ATTR_FRM_UL_SPACE:
    f << "ulSpace,";
    f << "upper=" << input->readULong(2);
    val=(int) input->readULong(nVers==1 ? 2 : 1);
    if (val) f << ":" << val;
    f << ",";
    f << "lower=" << input->readULong(2);
    val=(int) input->readULong(nVers==1 ? 2 : 1);
    if (val) f << ":" << val;
    f << ",";
    break;
  case ATTR_FRM_PAGEDESC:
    f << "pageDesc,";
    if (nVers<1)
      f << "bAutor=" << input->readULong(1) << ",";
    if (nVers< 2)
      f << "nOff=" << input->readULong(2) << ",";
    else {
      unsigned long nOff;
      if (!input->readCompressedULong(nOff)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read nOff\n"));
        f << "###nOff,";
        break;
      }
      f << "nOff=" << nOff << ",";
    }
    f << "nIdx=" << input->readULong(2) << ",";
    break;
  case ATTR_FRM_BREAK:
    f << "pageBreak=" << input->readULong(1) << ",";
    if (nVers<1) input->seek(1, librevenge::RVNG_SEEK_CUR); // dummy
    break;
  case ATTR_FRM_CNTNT: // checkme
    f << "pageCntnt,";
    while (input->tell()<lastPos) {
      long actPos=input->tell();
      if (input->peek()!='N' || !document.getSDWParser()->readSWContent(zone, document) || input->tell()<=actPos) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: find unknown pageCntnt child\n"));
        f << "###child";
        break;
      }
    }
    break;
  case ATTR_FRM_HEADER:
  case ATTR_FRM_FOOTER: {
    f << (nWhich==ATTR_FRM_HEADER ? "header" : "footer") << ",";
    f << "active=" << input->readULong(1) << ",";
    long actPos=input->tell();
    if (actPos==lastPos)
      break;
    SWFormatManager formatManager;
    formatManager.readSWFormatDef(zone,'r',document);
    break;
  }
  case ATTR_FRM_PRINT:
    f << "print=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_OPAQUE:
    f << "opaque=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_PROTECT:
    f << "protect,";
    val=(int) input->readULong(1);
    if (val&1) f << "pos[protect],";
    if (val&2) f << "size[protect],";
    if (val&4) f << "cntnt[protect],";
    break;
  case ATTR_FRM_SURROUND:
    f << "surround=" << input->readULong(1) << ",";
    if (nVers<5) f << "bGold=" << input->readULong(1) << ",";
    if (nVers>1) f << "bAnch=" << input->readULong(1) << ",";
    if (nVers>2) f << "bCont=" << input->readULong(1) << ",";
    if (nVers>3) f << "bOutside1=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_VERT_ORIENT:
  case ATTR_FRM_HORI_ORIENT:
    f << (nWhich==ATTR_FRM_VERT_ORIENT ? "vertOrient" : "horiOrient") << ",";
    f << "nPos=" << input->readULong(4) << ",";
    f << "nOrient=" << input->readULong(1) << ",";
    f << "nRel=" << input->readULong(1) << ",";
    if (nWhich==ATTR_FRM_HORI_ORIENT && nVers>=1) f << "bToggle=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_ANCHOR:
    f << "anchor=" << input->readULong(1) << ",";
    if (nVers<1)
      f << "nId=" << input->readULong(2) << ",";
    else {
      unsigned long nId;
      if (!input->readCompressedULong(nId)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read nId\n"));
        f << "###nId,";
        break;
      }
      else
        f << "nId=" << nId << ",";
    }
    break;
  // ATTR_FRM_BACKGROUND see case ATTR_CHR_BACKGROUND
  case ATTR_FRM_BOX:
  case ATTR_SC_BORDER: {
    f << (nWhich==ATTR_FRM_BOX ? "box" : "scBorder") << ",";
    f << "nDist=" << input->readULong(2) << ",";
    int cLine=0;
    bool ok=true;
    while (input->tell()<lastPos) {
      cLine=(int) input->readULong(1);
      if (cLine>3) break;
      f << "[";
      STOFFColor color;
      if (!input->readColor(color)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find a box's color\n"));
        f << "###color,";
        ok=false;
        break;
      }
      else if (!color.isBlack())
        f << "col=" << color << ",";
      f << "outline=" << input->readULong(2) << ",";
      f << "inline=" << input->readULong(2) << ",";
      f << "nDist=" << input->readULong(2) << ",";
      f << "],";
    }
    if (!ok) break;
    if (nVers>=1 && cLine&0x10) {
      f << "dist=[";
      for (int i=0; i<4; ++i) f << input->readULong(2) << ",";
      f << "],";
    }
    break;
  }
  case ATTR_FRM_SHADOW: {
    f << "shadow,";
    f << "cLoc=" << input->readULong(1) << ",";
    f << "nWidth=" << input->readULong(2) << ",";
    f << "bTrans=" << input->readULong(1) << ",";
    STOFFColor color;
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read color\n"));
      f << "###color,";
      break;
    }
    else if (!color.isWhite())
      f << "color=" << color << ",";
    if (!input->readColor(color)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read fill color\n"));
      f << "###fillcolor,";
      break;
    }
    else if (!color.isWhite())
      f << "fillcolor=" << color << ",";
    f << "style=" << input->readULong(1) << ",";
    break;
  }
  case ATTR_FRM_FRMMACRO: { // macitem.cxx SvxMacroTableDtor::Read
    f << "frmMacro,";
    if (nVers>=1) {
      nVers=(uint16_t) input->readULong(2);
      f << "nVersion=" << nVers << ",";
    }
    int16_t nMacros;
    *input>>nMacros;
    f << "macros=[";
    for (int16_t i=0; i<nMacros; ++i) {
      uint16_t nCurKey, eType;
      bool ok=true;
      f << "[";
      *input>>nCurKey;
      if (nCurKey) f << "nCurKey=" << nCurKey << ",";
      for (int j=0; j<2; ++j) {
        librevenge::RVNGString text;
        if (!zone.readString(text)) {
          STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find a macro string\n"));
          f << "###string" << j << ",";
          ok=false;
          break;
        }
        else if (!text.empty())
          f << (j==0 ? "lib" : "mac") << "=" << text.cstr() << ",";
      }
      if (!ok) break;
      if (nVers>=1) {
        *input>>eType;
        if (eType) f << "eType=" << eType << ",";
      }
      f << "],";
    }
    f << "],";
    break;
  }
  case ATTR_FRM_COL: {
    f << "col,";
    f << "nLineAdj=" << input->readULong(1) << ",";
    f << "bOrtho=" << input->readULong(1) << ",";
    f << "nLineHeight=" << input->readULong(1) << ",";
    f << "nGutterWidth=" << input->readLong(2) << ",";
    int nWishWidth=(int) input->readULong(2);
    f << "nWishWidth=" << nWishWidth << ",";
    f << "nPenStyle=" << input->readULong(1) << ",";
    f << "nPenWidth=" << input->readLong(2) << ",";
    f << "nPenRed=" << (input->readULong(2)>>8) << ",";
    f << "nPenGreen=" << (input->readULong(2)>>8) << ",";
    f << "nPenBlue=" << (input->readULong(2)>>8) << ",";
    int nCol=(int) input->readULong(2);
    f << "N=" << nCol << ",";
    if (nWishWidth==0)
      break;
    if (input->tell()+10*nCol>lastPos) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: nCol is bad\n"));
      f << "###N,";
      break;
    }
    f << "[";
    for (int i=0; i<nCol; ++i) {
      f << "[";
      f << "nWishWidth=" << input->readULong(2) << ",";
      f << "nLeft=" << input->readULong(2) << ",";
      f << "nUpper=" << input->readULong(2) << ",";
      f << "nRight=" << input->readULong(2) << ",";
      f << "nBottom=" << input->readULong(2) << ",";
      f << "],";
    }
    f << "],";
    break;
  }
  case ATTR_FRM_KEEP:
    f << "keep=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_URL:
    f << "url,";
    if (!SDWParser::readSWImageMap(zone))
      break;
    if (nVers>=1) {
      librevenge::RVNGString text;
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the setName\n"));
        f << "###name1,";
        break;
      }
      else if (!text.empty())
        f << "name1=" << text.cstr() << ",";
    }
    break;
  case ATTR_FRM_EDIT_IN_READONLY:
    f << "editInReadOnly=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_LAYOUT_SPLIT:
    f << "layoutSplit=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_CHAIN:
    f << "chain,";
    if (nVers>0) {
      f << "prevIdx=" << input->readULong(2) << ",";
      f << "nextIdx=" << input->readULong(2) << ",";
    }
    break;
  case ATTR_FRM_TEXTGRID:
    f << "textgrid=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_LINENUMBER:
    f << "lineNumber,";
    f << "start=" << input->readULong(4) << ",";
    f << "count[lines]=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_FTN_AT_TXTEND:
  case ATTR_FRM_END_AT_TXTEND:
    f << (nWhich==ATTR_FRM_FTN_AT_TXTEND ? "ftnAtTextEnd" : "ednAtTextEnd") << "=" << input->readULong(1) << ",";
    if (nVers>0) {
      f << "offset=" << input->readULong(2) << ",";
      f << "fmtType=" << input->readULong(2) << ",";
      librevenge::RVNGString text;
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the prefix\n"));
        f << "###prefix,";
        break;
      }
      else if (!text.empty())
        f << "prefix=" << text.cstr() << ",";
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the suffix\n"));
        f << "###suffix,";
        break;
      }
      else if (!text.empty())
        f << "suffix=" << text.cstr() << ",";
    }
    break;
  case ATTR_FRM_COLUMNBALANCE:
    f << "columnBalance=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_FRAMEDIR:
  case ATTR_SC_WRITINGDIR:
    f << (nWhich==ATTR_FRM_FRAMEDIR ? "frameDir" : "ScWritingDir") << "=" << input->readULong(2) << ",";
    break;
  case ATTR_FRM_HEADER_FOOTER_EAT_SPACING:
    f << "headerFooterEatSpacing=" << input->readULong(1) << ",";
    break;
  case ATTR_FRM_FRMATTR_DUMMY9:
    f << "frmAtrDummy9" << input->readULong(1) << ",";
    break;
  // graphic attribute
  case ATTR_GRF_MIRRORGRF:
    f << "grfMirrorGrf=" << input->readULong(1) << ",";
    if (nVers>0) f << "nToggle=" << input->readULong(1) << ",";
    break;
  case ATTR_GRF_CROPGRF:
    f << "grfCropGrf,";
    f << "top=" << input->readLong(4) << ",";
    f << "left=" << input->readLong(4) << ",";
    f << "right=" << input->readLong(4) << ",";
    f << "bottom=" << input->readLong(4) << ",";
    break;
  // no saved ?
  case ATTR_GRF_ROTATION:
    f << "grfRotation,";
    break;
  case ATTR_GRF_LUMINANCE:
    f << "grfLuminance,";
    break;
  case ATTR_GRF_CONTRAST:
    f << "grfContrast,";
    break;
  case ATTR_GRF_CHANNELR:
    f << "grfChannelR,";
    break;
  case ATTR_GRF_CHANNELG:
    f << "grfChannelG,";
    break;
  case ATTR_GRF_CHANNELB:
    f << "grfChannelB,";
    break;
  case ATTR_GRF_GAMMA:
    f << "grfGamma,";
    break;
  case ATTR_GRF_INVERT:
    f << "grfInvert,";
    break;
  case ATTR_GRF_TRANSPARENCY:
    f << "grfTransparency,";
    break;
  case ATTR_GRF_DRAWMODE:
    f << "grfDrawMode,";
    break;
  case ATTR_GRF_DUMMY1:
    f << "grfDummy1" << input->readULong(1) << ",";
    break;
  case ATTR_GRF_DUMMY2:
    f << "grfDummy2" << input->readULong(1) << ",";
    break;
  case ATTR_GRF_DUMMY3:
    f << "grfDummy3" << input->readULong(1) << ",";
    break;
  case ATTR_GRF_DUMMY4:
    f << "grfDummy4" << input->readULong(1) << ",";
    break;
  case ATTR_GRF_DUMMY5:
    f << "grfDummy5" << input->readULong(1) << ",";
    break;

  case ATTR_BOX_FORMAT:
    f << "boxFormat=" << input->readULong(1) << ",";
    f << "nTmp=" << input->readULong(4) << ",";
    break;
  case ATTR_BOX_FORMULA: {
    f << "boxFormula,";
    librevenge::RVNGString text;
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the formula\n"));
      f << "###formula,";
      break;
    }
    else if (!text.empty())
      f << "formula=" << text.cstr() << ",";
    break;
  }
  case ATTR_BOX_VALUE:
    f << "boxAtrValue,";
    if (nVers==0) {
      librevenge::RVNGString text;
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not find the dValue\n"));
        f << "###dValue,";
        break;
      }
      else if (!text.empty())
        f << "dValue=" << text.cstr() << ",";
    }
    else {
      double res;
      bool isNan;
      if (!input->readDoubleReverted8(res, isNan)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a double\n"));
        f << "##value,";
      }
      else if (res<0||res>0)
        f << "value=" << res << ",";
    }
    break;

  case ATTR_SC_HYPHENATE:
    f << "scHyphenate=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_HORJUSTIFY:
  case ATTR_SC_VERJUSTIFY:
    // algitem.cxx Svx{Hor,Ver}JustifyItem::Create
    f << (nWhich==ATTR_SC_HORJUSTIFY ? "scHorJustify" : "scVerJustify") << "=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_INDENT:
    f << "scIndent=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_ORIENTATION:
    // algitem.cxx SvxOrientationItem::Create
    f << "scOrientation=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_ROTATE_VALUE:
    f << "scRotateValue=" << input->readLong(4) << ",";
    break;
  case ATTR_SC_ROTATE_MODE:
    // rotmodit.cxx SvxRotateModeItem::Create
    f << "scRotateMode=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_VERTICAL_ASIAN:
    f << "scVerticalAsian=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_LINEBREAK:
    f << "scLineBreak=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_MARGIN:
    // algItem SvxMarginItem::Create
    f << "scMargin,";
    f << "top=" << input->readLong(2) << ",";
    f << "left=" << input->readLong(2) << ",";
    f << "right=" << input->readLong(2) << ",";
    f << "bottom=" << input->readLong(2) << ",";
    break;
  case ATTR_SC_MERGE:
    // sc_attrib.cxx ScMergeAttr::Create
    f << "scMerge,";
    f << "nCol=" << input->readLong(2) << ",";
    f << "nRow=" << input->readLong(2) << ",";
    break;
  case ATTR_SC_MERGE_FLAG:
    f << "scMergeFlag=" << input->readLong(2) << ",";
    break;
  case ATTR_SC_VALUE_FORMAT:
    f << "scValueFormat=" << input->readULong(4) << ",";
    break;
  case ATTR_SC_PROTECTION:
    // sc_attrib.cxx ScProtectionAttr::Create
    f << "scProtection,";
    f << "bProtect=" << input->readLong(1) << ",";
    f << "bHFormula=" << input->readLong(1) << ",";
    f << "bHCell=" << input->readLong(1) << ",";
    f << "bHPrint=" << input->readLong(1) << ",";
    break;
  case ATTR_SC_BORDER_INNER: {
    // frmitem.cxx SvxBoxInfoItem::Create
    f << "scBorderInner,";
    int cFlags=(int) input->readULong(1);
    if (cFlags&1) f << "setTable,";
    if (cFlags&2) f << "setDist,";
    if (cFlags&4) f << "setMinDist,";
    f << "defDist=" << input->readULong(2) << ",";
    int n=0;
    while (input->tell()<lastPos) {
      int cLine=(int) input->readLong(1);
      if (cLine>1) break;
      f << "line" << n++ <<"=[";
      STOFFColor col;
      if (!input->readColor(col)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a color\n"));
        f << "###color,";
        break;
      }
      else if (!col.isBlack())
        f << "col=" << col << ",";
      f << "out=" << input->readLong(2) << ",";
      f << "in=" << input->readLong(2) << ",";
      f << "dist=" << input->readLong(2) << ",";
      f << "],";
    }
    break;
  }
  case ATTR_SC_VALIDDATA:
  case ATTR_SC_CONDITIONAL:
    f << (nWhich==ATTR_SC_VALIDDATA ? "scValidData" : "scConditional") << input->readULong(4) << ",";
    break;
  case ATTR_SC_PATTERN: {
    f << "pattern,";
    // sc_patattr.cxx ScPatternAttr::Create
    bool hasStyle;
    *input>>hasStyle;
    if (hasStyle) {
      f << "hasStyle,";
      librevenge::RVNGString text;
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a name\n"));
        f << "###name,";
        break;
      }
      else if (!text.empty())
        f << "famDummy=" << text.cstr() << ",";
    }
    // TODO
    static bool first=true;
    if (first) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: reading a pattern item is not implemented\n"));
      first=false;
    }
    f << "##";
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  case ATTR_SC_PAGE: {
    // svx_pageitem.cxx SvxPageItem::Create
    f << "page,";
    librevenge::RVNGString text;
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a name\n"));
      f << "###name,";
      break;
    }
    else if (!text.empty())
      f << "name=" << text.cstr() << ",";
    f << "type=" << input->readULong(1) << ",";
    f << "bLand="<< input->readULong(1) << ",";
    f << "nUse=" << input->readULong(2) << ",";
    break;
  }
  case ATTR_SC_PAGE_PAPERTRAY:
    f << "paper[tray]=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_PAGE_SIZE:
  case ATTR_SC_PAGE_MAXSIZE:
    f << (nWhich==ATTR_SC_PAGE_SIZE ? "page[sz]" : "maxPage[sz]") << "=" << input->readLong(4) << "x" << input->readLong(4) << ",";
    break;
  case ATTR_SC_PAGE_HORCENTER:
  case ATTR_SC_PAGE_VERCENTER:
  case ATTR_SC_PAGE_ON:
    f << (nWhich==ATTR_SC_PAGE_HORCENTER ? "scPageHor[center]" : nWhich==ATTR_SC_PAGE_VERCENTER ? "scPageVer[center]" : "pageOn")
      << "=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_PAGE_DYNAMIC:
  case ATTR_SC_PAGE_SHARED:
  case ATTR_SC_PAGE_NOTES:
    f << (nWhich==ATTR_SC_PAGE_DYNAMIC ? "page[dynamic]" : nWhich==ATTR_SC_PAGE_SHARED ? "page[shared]" : "page[note]")
      << "=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_PAGE_GRID:
  case ATTR_SC_PAGE_HEADERS:
    f << (nWhich==ATTR_SC_PAGE_GRID ? "page[grid]" : "page[headers]") << "=" << input->readULong(1) << ",";
    break;
  case ATTR_SC_PAGE_CHARTS:
  case ATTR_SC_PAGE_OBJECTS:
  case ATTR_SC_PAGE_DRAWINGS:
    f << (nWhich==ATTR_SC_PAGE_CHARTS ? "page[charts]" : nWhich==ATTR_SC_PAGE_OBJECTS ? "page[object]" : "page[drawings]");
    if (nVers==0) f << ",";
    else f << input->readULong(2) << ",";
    break;
  case ATTR_SC_PAGE_TOPDOWN:
    f << "scPageTopDown=" << input->readULong(1);
    break;
  case ATTR_SC_PAGE_SCALE:
  case ATTR_SC_PAGE_SCALETOPAGES:
  case ATTR_SC_PAGE_FIRSTPAGENO:
    f << (nWhich==ATTR_SC_PAGE_SCALE ? "page[scale]" : nWhich==ATTR_SC_PAGE_SCALETOPAGES ? "page[scaleToPage]" : "firstPageNo")
      << "=" << input->readULong(2) << ",";
    break;
  case ATTR_SC_PAGE_PRINTAREA:
  case ATTR_SC_PAGE_REPEATROW:
  case ATTR_SC_PAGE_REPEATCOL:
    f << (nWhich==ATTR_SC_PAGE_PRINTAREA ? "print[area]" : nWhich==ATTR_SC_PAGE_REPEATROW ? "repeat[row]" : "repeat[col]") << ",";
    // sc_attrib.cxx ScRangeItem::Create
    if (nVers==0) {
      uint16_t n, nColS, nRowS, nColE, nRowE;
      *input >> n >> nColS >> nRowS >> nColE >> nRowE;
      if (n>255) f << "allTabs,";
      f << "range=" << nColS << "x" << nRowS << "<->" << nColE << "x" << nRowE << ",";
    }
    else {
      uint16_t nColS, nRowS, nColE, nRowE;
      *input >> nColS >> nRowS >> nColE >> nRowE;
      f << "range=" << nColS << "x" << nRowS << "<->" << nColE << "x" << nRowE << ",";
      if (nVers>=2) {
        bool newFlag;
        *input>>newFlag;
        if (newFlag) f << "newFlag,";
        if (input->tell()+1==lastPos) { // checkme
          *input>>newFlag;
          if (newFlag) f << "newFlag1,";
        }
      }
    }
    break;
  case ATTR_SC_PAGE_PRINTTABLES: {
    f << "print[tables],";
    uint16_t n;
    *input >> n;
    if (!n||input->tell()+2*int(n)>lastPos) break;
    f << "list=[";
    for (int i=0; i<int(n); ++i)
      f << input->readULong(2) << ",";
    f << "],";
    break;
  }
  case ATTR_SC_PAGE_HEADERLEFT:
  case ATTR_SC_PAGE_FOOTERLEFT:
  case ATTR_SC_PAGE_HEADERRIGHT:
  case ATTR_SC_PAGE_FOOTERRIGHT: {
    f << (nWhich==ATTR_SC_PAGE_HEADERLEFT ? "header[left]" :
          nWhich==ATTR_SC_PAGE_FOOTERLEFT ? "footer[left]" :
          nWhich==ATTR_SC_PAGE_HEADERRIGHT ? "header[right]" : "footer[right]") << ",";
    for (int i=0; i<3; ++i) {
      long actPos=input->tell();
      StarFileManager fileManager;
      if (!fileManager.readEditTextObject(zone, lastPos, document) || input->tell()>lastPos) {
        STOFF_DEBUG_MSG(("StarAttribute::readAttribute: can not read a text object\n"));
        ascFile.addPos(actPos);
        ascFile.addNote("StarAttribute:###editTextObject");
        input->seek(lastPos, librevenge::RVNG_SEEK_SET);
        break;
      }
    }
    break;
  }
  case ATTR_SC_PAGE_HEADERSET:
  case ATTR_SC_PAGE_FOOTERSET: {
    // svx_pageitem.cxx SvxSetItem::Create
    f << (nWhich==ATTR_SC_PAGE_HEADERSET ? "page[headerSet]" : "page[footerSet]") << ",";
    // TODO
    static bool first=true;
    if (first) {
      STOFF_DEBUG_MSG(("StarAttribute::readAttribute: reading a header/footer set item is not implemented\n"));
      first=false;
    }
    f << "##";
    ascFile.addDelimiter(input->tell(),'|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
    break;
  }
  case ATTR_SC_PAGE_FORMULAS:
  case ATTR_SC_PAGE_NULLVALS:
    f << (nWhich==ATTR_SC_PAGE_FORMULAS ? "page[formulas]" : "page[numVals") << "=" << input->readULong(1) << ",";
    break;
  default:
    STOFF_DEBUG_MSG(("StarAttribute::readAttribute: reading not format attribute is not implemented\n"));
    f << "#unimplemented[wh=" << std::hex << nWhich << std::dec << ",";
  }
  if (input->tell()!=lastPos) {
    STOFF_DEBUG_MSG(("StarAttribute::readAttribute: find extra data\n"));
    f << "###extra,";
    ascFile.addDelimiter(input->tell(), '|');
    input->seek(lastPos, librevenge::RVNG_SEEK_SET);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;

}

// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:
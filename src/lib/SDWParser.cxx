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

#include "STOFFOLEParser.hxx"

#include "SDCParser.hxx"
#include "StarAttribute.hxx"
#include "SWFieldManager.hxx"
#include "SWFormatManager.hxx"
#include "StarDocument.hxx"
#include "StarFileManager.hxx"
#include "StarItemPool.hxx"
#include "StarZone.hxx"

#include "SDWParser.hxx"

/** Internal: the structures of a SDWParser */
namespace SDWParserInternal
{
////////////////////////////////////////
//! Internal: the state of a SDWParser
struct State {
  //! constructor
  State() : m_actPage(0), m_numPages(0)
  {
  }

  int m_actPage /** the actual page */, m_numPages /** the number of page of the final document */;
};

}

////////////////////////////////////////////////////////////
// constructor/destructor, ...
////////////////////////////////////////////////////////////
SDWParser::SDWParser(STOFFInputStreamPtr input, STOFFHeader *header) :
  STOFFTextParser(input, header), m_password(0), m_oleParser(), m_state()
{
  init();
}

SDWParser::~SDWParser()
{
}

void SDWParser::init()
{
  setAsciiName("main-1");

  m_state.reset(new SDWParserInternal::State);
}

////////////////////////////////////////////////////////////
// the parser
////////////////////////////////////////////////////////////
void SDWParser::parse(librevenge::RVNGTextInterface *docInterface)
{
  if (!getInput().get() || !checkHeader(0L))  throw(libstoff::ParseException());
  bool ok = true;
  try {
    // create the asciiFile
    checkHeader(0L);
    ok = createZones();
    if (ok) {
      createDocument(docInterface);
    }
    ascii().reset();
  }
  catch (...) {
    STOFF_DEBUG_MSG(("SDWParser::parse: exception catched when parsing\n"));
    ok = false;
  }

  if (!ok) throw(libstoff::ParseException());
}


bool SDWParser::createZones()
{
  m_oleParser.reset(new STOFFOLEParser);
  m_oleParser->parse(getInput());

  // send the final data
  std::vector<shared_ptr<STOFFOLEParser::OleDirectory> > listDir=m_oleParser->getDirectoryList();
  for (size_t d=0; d<listDir.size(); ++d) {
    if (!listDir[d]) continue;
    shared_ptr<StarAttributeManager> attrManager(new StarAttributeManager);
    StarDocument document(getInput(), m_password, m_oleParser, listDir[d], attrManager, this);
    // Ole-Object has persist elements, so...
    if (listDir[d]->m_hasCompObj) document.parse();
    STOFFOLEParser::OleDirectory &direct=*listDir[d];
    std::vector<std::string> unparsedOLEs=direct.getUnparsedOles();
    size_t numUnparsed = unparsedOLEs.size();
    StarFileManager fileManager;
    for (size_t i = 0; i < numUnparsed; i++) {
      std::string const &name = unparsedOLEs[i];
      STOFFInputStreamPtr ole = getInput()->getSubStreamByName(name.c_str());
      if (!ole.get()) {
        STOFF_DEBUG_MSG(("SDWParser::createZones: error: can not find OLE part: \"%s\"\n", name.c_str()));
        continue;
      }

      std::string::size_type pos = name.find_last_of('/');
      std::string dir(""), base;
      if (pos == std::string::npos) base = name;
      else if (pos == 0) base = name.substr(1);
      else {
        dir = name.substr(0,pos);
        base = name.substr(pos+1);
      }
      ole->setReadInverted(true);
      if (base=="SwNumRules") {
        readSwNumRuleList(ole, name, document);
        continue;
      }
      if (base=="SwPageStyleSheets") {
        readSwPageStyleSheets(ole,name, document);
        continue;
      }

      if (base=="DrawingLayer") {
        readDrawingLayer(ole,name,document);
        continue;
      }
      if (base=="SfxStyleSheets") {
        SDCParser sdcParser;
        sdcParser.readSfxStyleSheets(ole,name,document);
        continue;
      }

      if (base=="StarCalcDocument") {
        SDCParser sdcParser;
        sdcParser.readCalcDocument(ole,name,document);
        continue;
      }
      if (base=="StarChartDocument") {
        SDCParser sdcParser;
        sdcParser.readChartDocument(ole,name,document);
        continue;
      }
      if (base=="StarImageDocument" || base=="StarImageDocument 4.0") {
        librevenge::RVNGBinaryData data;
        fileManager.readImageDocument(ole,data,name);
        continue;
      }
      if (base=="StarMathDocument") {
        fileManager.readMathDocument(ole,name,document);
        continue;
      }
      if (base=="StarWriterDocument") {
        readWriterDocument(ole,name, document);
        continue;
      }
      if (base.compare(0,3,"Pic")==0) {
        librevenge::RVNGBinaryData data;
        std::string type;
        fileManager.readEmbeddedPicture(ole,data,type,name,document);
        continue;
      }
      // other
      if (base=="Ole-Object") {
        fileManager.readOleObject(ole,name);
        continue;
      }
      libstoff::DebugFile asciiFile(ole);
      asciiFile.open(name);

      bool ok=false;
      if (base=="OutPlace Object")
        ok=fileManager.readOutPlaceObject(ole, asciiFile);
      if (!ok) {
        libstoff::DebugStream f;
        if (base=="Standard") // can be Standard or STANDARD
          f << "Entries(STANDARD):";
        else
          f << "Entries(" << base << "):";
        asciiFile.addPos(0);
        asciiFile.addNote(f.str().c_str());
      }
      asciiFile.reset();
    }
  }
  return false;
}

////////////////////////////////////////////////////////////
// create the document
////////////////////////////////////////////////////////////
void SDWParser::createDocument(librevenge::RVNGTextInterface *documentInterface)
{
  if (!documentInterface) return;
  STOFF_DEBUG_MSG(("SDWParser::createDocument: not implemented exist\n"));
  return;
}


////////////////////////////////////////////////////////////
//
// Intermediate level
//
////////////////////////////////////////////////////////////
bool SDWParser::readSwNumRuleList(STOFFInputStreamPtr input, std::string const &name, StarDocument &doc)
try
{
  StarZone zone(input, name, "SWNumRuleList", doc.getPassword());
  if (!zone.readSWHeader()) {
    STOFF_DEBUG_MSG(("SDWParser::readSwNumRuleList: can not read the header\n"));
    return false;
  }
  zone.readStringsPool();
  // sw_sw3num.cxx::Sw3IoImp::InNumRules
  libstoff::DebugFile &ascFile=zone.ascii();
  while (!input->isEnd()) {
    long pos=input->tell();
    int rType=input->peek();
    if ((rType=='0' || rType=='R') && readSWNumRule(zone, char(rType)))
      continue;
    char type;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    libstoff::DebugStream f;
    f << "SWNumRuleList[" << type << "]:";
    bool done=false;
    switch (type) {
    case '+': { // extra outline
      zone.openFlagZone();
      int N=(int) input->readULong(1);
      f << "N=" << N << ",";
      zone.closeFlagZone();
      if (input->tell()+3*N>zone.getRecordLastPosition()) {
        STOFF_DEBUG_MSG(("SDWParser::readSwNumRuleList: nExtraOutline seems bad\n"));
        f << "###,";
        break;
      }
      f << "levSpace=[";
      for (int i=0; i<N; ++i)
        f << input->readULong(1) << ":" << input->readULong(2) << ",";
      f << "],";
      break;
    }
    case '?':
      STOFF_DEBUG_MSG(("SDWParser::readSwNumRuleList: reading inHugeRecord(TEST_HUGE_DOCS) is not implemented\n"));
      break;
    case 'Z':
      done=true;
      break;
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSwNumRuleList: find unimplemented type\n"));
      f << "###type,";
      break;
    }
    if (!zone.closeSWRecord(type, "SWNumRuleList")) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    if (done)
      break;
  }
  if (!input->isEnd()) {
    STOFF_DEBUG_MSG(("SDWParser::readSwNumRuleList: find extra data\n"));
    ascFile.addPos(input->tell());
    ascFile.addNote("SWNumRuleList:###extra");
  }
  return true;
}
catch (...)
{
  return false;
}

bool SDWParser::readSwPageStyleSheets(STOFFInputStreamPtr input, std::string const &name, StarDocument &doc)
try
{
  StarZone zone(input, name, "SWPageStyleSheets", doc.getPassword());
  if (!zone.readSWHeader()) {
    STOFF_DEBUG_MSG(("SDWParser::readSwPageStyleSheets: can not read the header\n"));
    return false;
  }
  zone.readStringsPool();
  SWFieldManager fieldManager;
  while (fieldManager.readField(zone,'Y'))
    ;
  readSWBookmarkList(zone);
  readSWRedlineList(zone);
  SWFormatManager formatManager;
  formatManager.readSWNumberFormatterList(zone);

  // sw_sw3page.cxx Sw3IoImp::InPageDesc
  libstoff::DebugFile &ascFile=zone.ascii();
  while (!input->isEnd()) {
    long pos=input->tell();
    char type;
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    libstoff::DebugStream f;
    f << "SWPageStyleSheets[" << type << "]:";
    bool done=false;
    switch (type) {
    case 'P': {
      zone.openFlagZone();
      int N=(int) input->readULong(2);
      f << "N=" << N << ",";
      zone.closeFlagZone();
      for (int i=0; i<N; ++i) {
        if (!readSWPageDef(zone, doc))
          break;
      }
      break;
    }
    case 'Z':
      done=true;
      break;
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSwPageStyleSheets: find unknown data\n"));
      f << "###";
      break;
    }
    if (!zone.closeSWRecord(type, "SWPageStyleSheets")) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    if (done)
      break;
  }

  if (!input->isEnd()) {
    STOFF_DEBUG_MSG(("SDWParser::readSwPageStyleSheets: find extra data\n"));
    ascFile.addPos(input->tell());
    ascFile.addNote("SWPageStyleSheets:##extra");
  }

  return true;
}
catch (...)
{
  return false;
}

bool SDWParser::readSWPageDef(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='p' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3page.cxx InPageDesc
  libstoff::DebugStream f;
  f << "Entries(SWPageDef)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  if (fl&0xf0) f << "fl=" << (fl>>4) << ",";
  int val=(int) input->readULong(2);
  librevenge::RVNGString poolName;
  if (!zone.getPoolName(val, poolName)) {
    STOFF_DEBUG_MSG(("SDWParser::readSwPageDef: can not find a pool name\n"));
    f << "###nId=" << val << ",";
  }
  else if (!poolName.empty())
    f << poolName.cstr() << ",";
  val=(int) input->readULong(2);
  if (val) f << "nFollow=" << val << ",";
  val=(int) input->readULong(2);
  if (val) f << "nPoolId2=" << val << ",";
  val=(int) input->readULong(1);
  if (val) f << "nNumType=" << val << ",";
  val=(int) input->readULong(2);
  if (val) f << "nUseOn=" << val << ",";
  if (zone.isCompatibleWith(0x16,0x22, 0x101)) {
    val=(int) input->readULong(2);
    if (val!=0xffff) f << "regCollIdx=" << val << ",";
  }
  zone.closeFlagZone();

  long lastPos=zone.getRecordLastPosition();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  while (input->tell() < lastPos) {
    pos=input->tell();
    int rType=input->peek();
    if (rType=='S' && readSWAttributeList(zone, doc))
      continue;

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    f.str("");
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "SWPageDef[" << type << "-" << zone.getRecordLevel() << "]:";
    switch (type) {
    case '1': // foot info
    case '2': { // page foot info
      f << (type=='1' ? "footInfo" : "pageFootInfo") << ",";
      val=(int) input->readLong(4);
      if (val) f << "height=" << val << ",";
      val=(int) input->readLong(4);
      if (val) f << "topDist=" << val << ",";
      val=(int) input->readLong(4);
      if (val) f << "bottomDist=" << val << ",";
      val=(int) input->readLong(2);
      if (val) f << "adjust=" << val << ",";
      f << "width=" << input->readLong(4) << "/" << input->readLong(4) << ",";
      val=(int) input->readLong(2);
      if (val) f << "penWidth=" << val << ",";
      STOFFColor col;
      if (!input->readColor(col)) {
        STOFF_DEBUG_MSG(("SDWParser::readSwPageDef: can not read a color\n"));
        f << "###color,";
      }
      else if (!col.isBlack())
        f << col << ",";
      break;
    }
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSwPageDef: find unknown type\n"));
      f << "###type,";
      break;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWPageDef");
  }
  zone.closeSWRecord('p', "SWPageDef");
  return true;
}

bool SDWParser::readSWAttribute(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='A' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  // sw_sw3fmts.cxx InAttr
  libstoff::DebugStream f;
  f << "Entries(StarAttribute)[SW-" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  uint16_t nWhich, nVers, nBegin=0xFFFF, nEnd=0xFFFF;
  *input >> nWhich >> nVers;
  if (fl&0x10) *input >> nBegin;
  if (fl&0x20) *input >> nEnd;

  int which=(int) nWhich;
  if (which>0x6001 && zone.getDocumentVersion()!=0x0219) // bug correction 0x95500
    which+=15;
  if (which>=0x1000 && which<=0x1024) which+=-0x1000+(int) StarAttribute::ATTR_CHR_CASEMAP;
  else if (which>=0x2000 && which<=0x2009) which+=-0x2000+(int) StarAttribute::ATTR_TXT_INETFMT;
  else if (which>=0x3000 && which<=0x3006) which+=-0x3000+(int) StarAttribute::ATTR_TXT_FIELD;
  else if (which>=0x4000 && which<=0x4013) which+=-0x4000+(int) StarAttribute::ATTR_PARA_LINESPACING;
  else if (which>=0x5000 && which<=0x5022) which+=-0x5000+(int) StarAttribute::ATTR_FRM_FILL_ORDER;
  else if (which>=0x6000 && which<=0x6013) which+=-0x6000+(int) StarAttribute::ATTR_GRF_MIRRORGRF;
  else {
    STOFF_DEBUG_MSG(("SDWParser::readSWAttribute: find unexpected which value\n"));
    which=-1;
    f << "###";
  }
  f << "wh=" << which << "[" << std::hex << nWhich << std::dec << "],";
  if (nVers) f << "nVers=" << nVers << ",";
  if (nBegin!=0xFFFF) f << "nBgin=" << nBegin << ",";
  if (nEnd!=0xFFFF) f << "nEnd=" << nEnd << ",";
  zone.closeFlagZone();

  if (which<=0 || !doc.getAttributeManager() ||
      !doc.getAttributeManager()->readAttribute(zone, which, int(nVers), zone.getRecordLastPosition(), doc))
    f << "###";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('A', "StarAttribute");
  return true;
}

bool SDWParser::readSWAttributeList(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='S' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  libstoff::DebugStream f;
  f << "Entries(StarAttribute)[SWList-" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  while (input->tell() < zone.getRecordLastPosition()) { // normally only 2
    pos=input->tell();
    if (readSWAttribute(zone, doc) && input->tell()>pos) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  zone.closeSWRecord('S', "StarAttribute");
  return true;
}

bool SDWParser::readSWBookmarkList(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='a' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  // sw_sw3misc.cxx InBookmarks
  libstoff::DebugStream f;
  f << "Entries(SWBookmark)[" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  while (input->tell()<zone.getRecordLastPosition()) {
    pos=input->tell();
    f.str("");
    f << "SWBookmark:";
    if (input->peek()!='B' || !zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }

    librevenge::RVNGString text("");
    bool ok=true;
    if (!zone.readString(text)) {
      ok=false;
      STOFF_DEBUG_MSG(("SDWParser::readSWBookmarkList: can not read shortName\n"));
      f << "###short";
    }
    else
      f << text.cstr();
    if (ok && !zone.readString(text)) {
      ok=false;
      STOFF_DEBUG_MSG(("SDWParser::readSWBookmarkList: can not read name\n"));
      f << "###";
    }
    else
      f << text.cstr();
    if (ok) {
      zone.openFlagZone();
      int val=(int) input->readULong(2);
      if (val) f << "nOffset=" << val << ",";
      val=(int) input->readULong(2);
      if (val) f << "nKey=" << val << ",";
      val=(int) input->readULong(2);
      if (val) f << "nMod=" << val << ",";
      zone.closeFlagZone();
    }
    if (ok && input->tell()<zone.getRecordLastPosition()) {
      for (int i=0; i<4; ++i) { // start[aMac:aLib],end[aMac:Alib]
        if (!zone.readString(text)) {
          STOFF_DEBUG_MSG(("SDWParser::readSWBookmarkList: can not read macro name\n"));
          f << "###macro";
          break;
        }
        else if (!text.empty())
          f << "macro" << i << "=" << text.cstr();
      }
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWBookmark");
  }

  zone.closeSWRecord('a', "SWBookmark");
  return true;
}

bool SDWParser::readSWContent(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='N' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3sectn.cxx: InContents
  libstoff::DebugStream f;
  f << "Entries(SWContent)[" << zone.getRecordLevel() << "]:";
  if (zone.isCompatibleWith(5))
    zone.openFlagZone();
  int nNodes;
  if (zone.isCompatibleWith(0x201))
    nNodes=(int) input->readULong(4);
  else {
    if (zone.isCompatibleWith(5))
      f << "sectId=" << input->readULong(2) << ",";
    nNodes=(int) input->readULong(2);
  }
  f << "N=" << nNodes << ",";
  if (zone.isCompatibleWith(5))
    zone.closeFlagZone();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long lastPos=zone.getRecordLastPosition();
  SWFormatManager formatManager;
  for (int i=0; i<nNodes; ++i) {
    if (input->tell()>=lastPos) break;
    pos=input->tell();
    int cType=input->peek();
    bool done=false;
    switch (cType) {
    case 'E':
      done=readSWTable(zone, doc);
      break;
    case 'G':
      done=readSWGraphNode(zone, doc);
      break;
    case 'I':
      done=readSWSection(zone);
      break;
    case 'O':
      done=readSWOLENode(zone);
      break;
    case 'T':
      done=readSWTextZone(zone, doc);
      break;
    case 'l': // related to link
    case 'o': // format: safe to ignore
      done=formatManager.readSWFormatDef(zone,char(cType),doc);
      break;
    case 'v':
      done=readSWNodeRedline(zone);
      break;
    default:
      break;
    }
    if (done) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "SWContent[" << type << "-" << zone.getRecordLevel() << "]:";
    switch (cType) {
    case 'i':
      // sw_sw3node.cxx InRepTxtNode
      f << "repTxtNode,";
      f << "rep=" << input->readULong(4) << ",";
      break;
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSWContent: find unexpected type\n"));
      f << "###";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWContent");
  }
  zone.closeSWRecord('N', "SWContent");
  return true;
}

bool SDWParser::readSWDBName(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='D' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3num.cxx: InDBName
  libstoff::DebugStream f;
  f << "Entries(SWDBName)[" << zone.getRecordLevel() << "]:";
  librevenge::RVNGString text("");
  if (!zone.readString(text)) {
    STOFF_DEBUG_MSG(("SDWParser::readSWDBName: can not read a string\n"));
    f << "###string";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord('D', "SWDBName");
    return true;
  }
  if (!text.empty())
    f << "sStr=" << text.cstr() << ",";
  if (zone.isCompatibleWith(0xf,0x101)) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWDBName: can not read a SQL string\n"));
      f << "###SQL";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('D', "SWDBName");
      return true;
    }
    if (!text.empty())
      f << "sSQL=" << text.cstr() << ",";
  }
  if (zone.isCompatibleWith(0x11,0x22)) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWDBName: can not read a table name string\n"));
      f << "###tableName";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('D', "SWDBName");
      return true;
    }
    if (!text.empty())
      f << "sTableName=" << text.cstr() << ",";
  }
  if (zone.isCompatibleWith(0x12,0x22, 0x101)) {
    int nCount=(int) input->readULong(2);
    f << "nCount=" << nCount << ",";
    if (nCount>0 && zone.isCompatibleWith(0x28)) {
      f << "dbData=[";
      for (int i=0; i<nCount; ++i) {
        if (input->tell()>=zone.getRecordLastPosition()) {
          STOFF_DEBUG_MSG(("SDWParser::readSWDBName: can not read a DBData\n"));
          f << "###";
          break;
        }
        if (!zone.readString(text)) {
          STOFF_DEBUG_MSG(("SDWParser::readSWDBName: can not read a table name string\n"));
          f << "###dbDataName";
          break;
        }
        f << text.cstr() << ":";
        f << input->readULong(4) << "<->"; // selStart
        f << input->readULong(4) << ","; // selEnd
      }
      f << "],";
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('D', "SWDBName");
  return true;
}

bool SDWParser::readSWDictionary(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='j' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3misc.cxx: InDictionary
  libstoff::DebugStream f;
  f << "Entries(SWDictionary)[" << zone.getRecordLevel() << "]:";
  long lastPos=zone.getRecordLastPosition();
  librevenge::RVNGString string;
  while (input->tell()<lastPos) {
    pos=input->tell();
    f << "[";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWDictionary: can not read a string\n"));
      f << "###string,";
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    if (!string.empty())
      f << string.cstr() << ",";
    f << "nLanguage=" << input->readULong(2) << ",";
    f << "nCount=" << input->readULong(2) << ",";
    f << "c=" << input->readULong(1) << ",";
    f << "],";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  zone.closeSWRecord('j', "SWDictionary");
  return true;
}

bool SDWParser::readSWEndNoteInfo(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='4' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3num.cxx: InEndNoteInfo
  libstoff::DebugStream f;
  f << "Entries(SWEndNoteInfo)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  f << "eType=" << input->readULong(1) << ",";
  f << "nPageId=" << input->readULong(2) << ",";
  f << "nCollIdx=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0xc))
    f << "nFtnOffset=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x203))
    f << "nChrIdx=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x216) && (fl&0x10))
    f << "nAnchorChrIdx=" << input->readULong(2) << ",";
  zone.closeFlagZone();

  if (zone.isCompatibleWith(0x203)) {
    librevenge::RVNGString text("");
    for (int i=0; i<2; ++i) {
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWEndNoteInfo: can not read a string\n"));
        f << "###string";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord('4', "SWEndNoteInfo");
        return true;
      }
      if (!text.empty())
        f << (i==0 ? "prefix":"suffix") << "=" << text.cstr() << ",";
    }
  }
  if (input->tell()<zone.getRecordLastPosition()) {
    STOFF_DEBUG_MSG(("SDWParser::readSWEndNoteInfo: find extra data\n"));
    f << "###";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('4', "SWEndNoteInfo");
  return true;
}

bool SDWParser::readSWFootNoteInfo(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='1' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3num.cxx: InFtnInfo and InFntInfo40
  libstoff::DebugStream f;
  f << "Entries(SWFootNoteInfo)[" << zone.getRecordLevel() << "]:";
  bool old=!zone.isCompatibleWith(0x201);
  librevenge::RVNGString text("");
  if (old) {
    for (int i=0; i<2; ++i) {
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWFootNoteInfo: can not read a string\n"));
        f << "###string";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord('1', "SWFootNoteInfo");
      }
      if (!text.empty())
        f << (i==0 ? "quoVadis":"ergoSum") << "=" << text.cstr() << ",";
    }
  }
  int fl=zone.openFlagZone();

  if (old) {
    f << "ePos=" << input->readULong(1) << ",";
    f << "eNum=" << input->readULong(1) << ",";
  }
  f << "eType=" << input->readULong(1) << ",";
  f << "nPageId=" << input->readULong(2) << ",";
  f << "nCollIdx=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0xc))
    f << "nFtnOffset=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x203))
    f << "nChrIdx=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x216) && (fl&0x10))
    f << "nAnchorChrIdx=" << input->readULong(2) << ",";
  zone.closeFlagZone();

  if (zone.isCompatibleWith(0x203)) {
    for (int i=0; i<2; ++i) {
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWFootNoteInfo: can not read a string\n"));
        f << "###string";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord('1', "SWFootNoteInfo");
        return true;
      }
      if (!text.empty())
        f << (i==0 ? "prefix":"suffix") << "=" << text.cstr() << ",";
    }
  }

  if (!old) {
    zone.openFlagZone();
    f << "ePos=" << input->readULong(1) << ",";
    f << "eNum=" << input->readULong(1) << ",";
    zone.closeFlagZone();
    for (int i=0; i<2; ++i) {
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWFootNoteInfo: can not read a string\n"));
        f << "###string";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord('1', "SWFootNoteInfo");
        return true;
      }
      if (!text.empty())
        f << (i==0 ? "quoVadis":"ergoSum") << "=" << text.cstr() << ",";
    }
  }
  if (input->tell()<zone.getRecordLastPosition()) {
    STOFF_DEBUG_MSG(("SDWParser::readSWFootNoteInfo: find extra data\n"));
    f << "###";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('1', "SWFootNoteInfo");
  return true;
}

bool SDWParser::readSWGraphNode(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='G' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3nodes.cxx: InGrfNode
  libstoff::DebugStream f;
  f << "Entries(SWGraphNode)[" << zone.getRecordLevel() << "]:";

  librevenge::RVNGString text;
  int fl=zone.openFlagZone();
  if (fl&0x10) f << "link,";
  if (fl&0x20) f << "empty,";
  if (fl&0x40) f << "serverMap,";
  zone.closeFlagZone();
  for (int i=0; i<2; ++i) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWGraphNode: can not read a string\n"));
      f << "###string";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('G', "SWGraphNode");
      return true;
    }
    if (!text.empty())
      f << (i==0 ? "grfName" : "fltName") << "=" << text.cstr() << ",";
  }
  if (zone.isCompatibleWith(0x101)) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWGraphNode: can not read a objName\n"));
      f << "###textRepl";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('G', "SWGraphNode");
      return true;
    }
    if (!text.empty())
      f << "textRepl=" << text.cstr() << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  long lastPos=zone.getRecordLastPosition();
  while (input->tell() < lastPos) {
    pos=input->tell();
    bool done=false;
    int rType=input->peek();

    switch (rType) {
    case 'S':
      done=readSWAttributeList(zone, doc);
      break;
    case 'X':
      done=readSWImageMap(zone);
      break;
    default:
      break;
    }
    if (done)
      continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "SWGraphNode[" << type << "-" << zone.getRecordLevel() << "]:";
    switch (type) {
    case 'k': {
      // sw_sw3nodes.cxx InContour
      int polyFl=zone.openFlagZone();
      zone.closeFlagZone();
      if (polyFl&0x10) {
        // poly2.cxx operator>>
        int numPoly=(int) input->readULong(2);
        for (int i=0; i<numPoly; ++i) {
          f << "poly" << i << "=[";
          // poly.cxx operator>>
          int numPoints=(int) input->readULong(2);
          if (input->tell()+8*numPoints>lastPos) {
            STOFF_DEBUG_MSG(("SDWParser::readSWGraphNode: can not read a polygon\n"));
            f << "###poly";
            break;
          }
          for (int p=0; p<numPoints; ++p)
            f << input->readLong(4) << "x" << input->readLong(4) << ",";
          f << "],";
        }
      }
      break;
    }
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSWGraphNode: find unexpected type\n"));
      f << "###";
      break;
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWGraphNode");
  }

  zone.closeSWRecord('G', "SWGraphNode");
  return true;
}

bool SDWParser::readSWImageMap(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='X' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugStream f;
  f << "Entries(SWImageMap)[" << zone.getRecordLevel() << "]:";
  // sw_sw3nodes.cxx InImageMap
  int flag=zone.openFlagZone();
  if (flag&0xF0) f << "fl=" << flag << ",";
  zone.closeFlagZone();
  librevenge::RVNGString string;
  if (!zone.readString(string)) {
    STOFF_DEBUG_MSG(("SDWParser::readSWImageMap: can not read url\n"));
    f << "###url";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    zone.closeSWRecord('X', "SWImageMap");
    return true;
  }
  if (!string.empty())
    f << "url=" << string.cstr() << ",";
  if (zone.isCompatibleWith(0x11,0x22, 0x101)) {
    for (int i=0; i<2; ++i) {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWImageMap: can not read string\n"));
        f << "###string";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());

        zone.closeSWRecord('X', "SWImageMap");
        return true;
      }
      if (string.empty()) continue;
      f << (i==0 ? "target" : "dummy") << "=" << string.cstr() << ",";
    }
  }
  if (flag&0x20) {
    // svt_imap.cxx: ImageMap::Read
    std::string cMagic("");
    for (int i=0; i<6; ++i) cMagic+=(char) input->readULong(1);
    if (cMagic!="SDIMAP") {
      STOFF_DEBUG_MSG(("SDWParser::readSWImageMap: cMagic is bad\n"));
      f << "###cMagic=" << cMagic << ",";
    }
    else {
      input->seek(2, librevenge::RVNG_SEEK_CUR);
      for (int i=0; i<3; ++i) {
        if (!zone.readString(string)) {
          STOFF_DEBUG_MSG(("SDWParser::readSWImageMap: can not read string\n"));
          f << "###string";
          ascFile.addPos(pos);
          ascFile.addNote(f.str().c_str());

          zone.closeSWRecord('X', "SWImageMap");
          return true;
        }
        if (!string.empty())
          f << (i==0 ? "target" : i==1 ? "dummy1" : "dummy2") << "=" << string.cstr() << ",";
        if (i==1)
          f << "nCount=" << input->readULong(2) << ",";
      }
      if (input->tell()<zone.getRecordLastPosition()) {
        STOFF_DEBUG_MSG(("SDWParser::readSWImageMap: find imapCompat data, not implemented\n"));
        // svt_imap3.cxx IMapCompat::IMapCompat
        ascFile.addPos(input->tell());
        ascFile.addNote("SWImageMap:###IMapCompat");
        input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
      }
    }
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('X', "SWImageMap");
  return true;
}

bool SDWParser::readSWJobSetUp(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='J' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  libstoff::DebugStream f;
  zone.openFlagZone();
  zone.closeFlagZone();
  if (input->tell()==zone.getRecordLastPosition()) // empty
    f << "Entries(JobSetUp)[" << zone.getRecordLevel() << "]:";
  else {
    f << "JobSetUp[container-" << zone.getRecordLevel() << "]:";
    StarFileManager fileManager;
    fileManager.readJobSetUp(zone);
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  zone.closeSWRecord(type, "JobSetUp[container]");
  return true;
}

bool SDWParser::readSWLayoutInfo(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='U' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // find no code, let improvise
  libstoff::DebugStream f;
  f << "Entries(SWLayoutInfo)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  if (fl&0xf0) f << "fl=" << (fl>>4) << ",";
  f << "f0=" << input->readULong(2) << ",";
  if (input->tell()!=zone.getFlagLastPosition()) // maybe
    f << "f1=" << input->readULong(2) << ",";
  zone.closeFlagZone();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long lastPos=zone.getRecordLastPosition();
  while (input->tell()<lastPos) {
    pos=input->tell();
    if (readSWLayoutSub(zone)) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "SWLayoutInfo[" << std::hex << int((unsigned char)type) << std::dec << "]:";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWLayoutInfo");
    break;
  }

  zone.closeSWRecord('U', "SWLayoutInfo");
  return true;
}

bool SDWParser::readSWLayoutSub(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  int rType=input->peek();
  if ((rType!=0xd2&&rType!=0xd7) || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }

  // find no code, let improvise
  libstoff::DebugStream f;
  f << "Entries(SWLayoutSub)[" << std::hex << rType << std::dec << "-" << zone.getRecordLevel() << "]:";
  int const expectedSz=rType==0xd2 ? 11 : 9;
  long lastPos=zone.getRecordLastPosition();
  int val=(int) input->readULong(1);
  if (val!=0x11) f << "f0=" << val << ",";
  val=(int) input->readULong(1);
  if (val!=0xaf) f << "f1=" << val << ",";
  val=(int) input->readULong(1); // small value 1-1f
  if (val) f << "f2=" << val << ",";
  val=(int) input->readULong(1);
  if (val) f << "f3=" << std::hex << val << std::dec << ",";
  if (input->tell()+(val&0xf)+expectedSz>lastPos) {
    STOFF_DEBUG_MSG(("SDWParser::readSWLayoutSub: the zone seems too short\n"));
    f << "###";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(char(0xd2), "SWLayoutSub");
    return true;
  }
  ascFile.addDelimiter(input->tell(),'|');
  input->seek(input->tell()+(val&0xf)+expectedSz, librevenge::RVNG_SEEK_SET);
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  while (input->tell()<lastPos) {
    pos=input->tell();
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "SWLayoutSub[" << std::hex << int((unsigned char)type) << std::dec << "]:";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    input->seek(zone.getRecordLastPosition(), librevenge::RVNG_SEEK_SET);
    zone.closeSWRecord(type, "SWLayoutSub");
  }

  zone.closeSWRecord(char(rType), "SWLayoutSub");
  return true;
}

bool SDWParser::readSWMacroTable(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='M' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3misc.cxx: InMacroTable
  libstoff::DebugStream f;
  f << "Entries(SWMacroTable)[" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  long lastPos=zone.getRecordLastPosition();
  librevenge::RVNGString string;
  while (input->tell()<lastPos) {
    pos=input->tell();
    if (input->peek()!='m' || !zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "SWMacroTable:";
    f << "key=" << input->readULong(2) << ",";
    bool ok=true;
    for (int i=0; i<2; ++i) {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWMacroTable: can not read a string\n"));
        f << "###,";
        ok=false;
        break;
      }
      if (!string.empty())
        f << string.cstr() << ",";
    }
    if (ok && zone.isCompatibleWith(0x102))
      f << "scriptType=" << input->readULong(2) << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWMacroTable");
  }
  zone.closeSWRecord('M', "SWMacroTable");
  return true;
}

bool SDWParser::readSWNodeRedline(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='v' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3redln.cxx InNodeRedLine
  libstoff::DebugStream f;
  f << "Entries(SWNodeRedline)[" << zone.getRecordLevel() << "]:";
  int cFlag=zone.openFlagZone();
  if (cFlag&0xf0) f << "flag=" << (cFlag>>4) << ",";
  f << "nId=" << input->readULong(2) << ",";
  f << "nNodeOf=" << input->readULong(2) << ",";
  zone.closeFlagZone();

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('v', "SWNodeRedline");
  return true;
}

bool SDWParser::readSWNumRule(StarZone &zone, char cKind)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!=cKind || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3num.cxx::Sw3IoImp::InNumRule
  libstoff::DebugStream f;
  f << "Entries(SWNumRuleDef)[" << cKind << "-" << zone.getRecordLevel() << "]:";
  int cFlags=0x20, nPoolId=-1, nStringId=0xFFFF;
  int val;
  if (zone.isCompatibleWith(0x201)) {
    cFlags=(int) zone.openFlagZone();
    nStringId=(int) input->readULong(2);
    librevenge::RVNGString poolName;
    if (nStringId==0xFFFF)
      ;
    else if (!zone.getPoolName(nStringId, poolName))
      f << "###nStringId=" << nStringId << ",";
    else if (!poolName.empty())
      f << poolName.cstr() << ",";
    if (cFlags&0x10) {
      nPoolId=(int) input->readULong(2);
      f << "PoolId=" << nPoolId << ",";
      val=(int) input->readULong(2);
      if (val) f << "poolHelpId=" << val << ",";
      val=(int) input->readULong(1);
      if (val) f << "poolHelpFileId=" << val << ",";
    }
  }
  val=(int) input->readULong(1);
  if (val) f << "eType=" << val << ",";
  if (zone.isCompatibleWith(0x201))
    zone.closeFlagZone();
  int nFormat=(int) input->readULong(1);
  long lastPos=zone.getRecordLastPosition();
  f << "nFormat=" << nFormat << ",";
  if (input->tell()+nFormat>lastPos) {
    STOFF_DEBUG_MSG(("SDWParser::readSwNumRule: nFormat seems bad\n"));
    f << "###,";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(cKind, "SWNumRuleDef");
    return true;
  }
  f << "lvl=[";
  for (int i=0; i<nFormat; ++i) f  << input->readULong(1) << ",";
  f << "],";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  int nKnownFormat=nFormat>10 ? 10 : nFormat;
  SWFormatManager formatManager;
  for (int i=0; i<nKnownFormat; ++i) {
    pos=input->tell();
    if (formatManager.readSWNumberFormat(zone)) continue;
    STOFF_DEBUG_MSG(("SDWParser::readSwNumRule: can not read a format\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }

  zone.closeSWRecord(cKind, "SWNumRuleDef");
  return true;
}

bool SDWParser::readSWOLENode(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='O' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3nodes.cxx: InOLENode
  libstoff::DebugStream f;
  f << "Entries(SWOLENode)[" << zone.getRecordLevel() << "]:";

  librevenge::RVNGString text;
  if (!zone.readString(text)) {
    STOFF_DEBUG_MSG(("SDWParser::readSWOLENode: can not read a objName\n"));
    f << "###objName";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord('O', "SWOLENode");
    return true;
  }
  if (!text.empty())
    f << "objName=" << text.cstr() << ",";
  if (zone.isCompatibleWith(0x101)) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWOLENode: can not read a objName\n"));
      f << "###textRepl";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('O', "SWOLENode");
      return true;
    }
    if (!text.empty())
      f << "textRepl=" << text.cstr() << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('O', "SWOLENode");
  return true;
}

bool SDWParser::readSWRedlineList(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='V' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3redline.cxx inRedlines
  libstoff::DebugStream f;
  f << "Entries(SWRedline)[" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  while (input->tell()<zone.getRecordLastPosition()) {
    pos=input->tell();
    f.str("");
    f << "SWRedline:";
    if (input->peek()!='R' || !zone.openSWRecord(type)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWRedlineList: find extra data\n"));
      f << "###";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      break;
    }
    zone.openFlagZone();
    int N=(int) input->readULong(2);
    zone.closeFlagZone();
    f << "N=" << N << ",";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());

    for (int i=0; i<N; ++i) {
      pos=input->tell();
      f.str("");
      f << "SWRedline-" << i << ":";
      if (input->peek()!='D' || !zone.openSWRecord(type)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWRedlineList: can not read a redline\n"));
        f << "###";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        break;
      }

      zone.openFlagZone();
      int val=(int) input->readULong(1);
      if (val) f << "cType=" << val << ",";
      val=(int) input->readULong(2);
      if (val) f << "stringId=" << val << ",";
      zone.closeFlagZone();

      f << "date=" << input->readULong(4) << ",";
      f << "time=" << input->readULong(4) << ",";
      librevenge::RVNGString text;
      if (!zone.readString(text)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWRedlineList: can not read the comment\n"));
        f << "###comment";
      }
      else if (!text.empty())
        f << text.cstr();
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('D', "SWRedline");
    }
    zone.closeSWRecord('R', "SWRedline");
  }
  zone.closeSWRecord('V', "SWRedline");
  return true;
}

bool SDWParser::readSWSection(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='I' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3sectn.cxx: InSection
  libstoff::DebugStream f;
  f << "Entries(SWSection)[" << zone.getRecordLevel() << "]:";

  librevenge::RVNGString text;
  for (int i=0; i<2; ++i) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWSection: can not read a string\n"));
      f << "###string";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('I', "SWSection");
      return true;
    }
    if (text.empty()) continue;
    f << (i==0 ? "name" : "cond") << "=" << text.cstr() << ",";
  }
  int fl=zone.openFlagZone();
  if (fl&0x10) f << "hidden,";
  if (fl&0x20) f << "protect,";
  if (fl&0x40) f << "condHidden,";
  if (fl&0x40) f << "connectFlag,";
  f << "nType=" << input->readULong(2) << ",";
  zone.closeFlagZone();
  if (zone.isCompatibleWith(0xd)) {
    if (!zone.readString(text)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWSection: can not read a linkName\n"));
      f << "###linkName";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord('I', "SWSection");
      return true;
    }
    else if (!text.empty())
      f << "linkName=" << text.cstr() << ",";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  zone.closeSWRecord('I', "SWSection");
  return true;
}

bool SDWParser::readSWTable(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='E' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3table.cxx: InTable
  libstoff::DebugStream f;
  f << "Entries(SWTableDef)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  if (fl&0x20) f << "headerRepeat,";
  f << "nBoxes=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x5,0x201))
    f << "nTblIdDummy=" << input->readULong(2) << ",";
  if (zone.isCompatibleWith(0x103))
    f << "cChgMode=" << input->readULong(1) << ",";
  zone.closeFlagZone();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long lastPos=zone.getRecordLastPosition();
  SWFormatManager formatManager;
  if (input->peek()=='f') formatManager.readSWFormatDef(zone, 'f', doc);
  if (input->peek()=='Y') {
    SWFieldManager fieldManager;
    fieldManager.readField(zone,'Y');
  }
  while (input->tell()<lastPos && input->peek()=='v') {
    pos=input->tell();
    if (readSWNodeRedline(zone))
      continue;
    STOFF_DEBUG_MSG(("SDWParser::readSWTable: can not read a red line\n"));
    ascFile.addPos(pos);
    ascFile.addNote("SWTableDef:###redline");
    zone.closeSWRecord('E',"SWTableDef");
    return true;
  }

  while (input->tell()<lastPos && input->peek()=='L') {
    pos=input->tell();
    if (readSWTableLine(zone, doc))
      continue;
    pos=input->tell();
    STOFF_DEBUG_MSG(("SDWParser::readSWTable: can not read a table line\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  zone.closeSWRecord('E',"SWTableDef");
  return true;
}

bool SDWParser::readSWTableBox(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='t' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3table.cxx: InTableBox
  libstoff::DebugStream f;
  f << "Entries(SWTableBox)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  if (fl&0x20 || !zone.isCompatibleWith(0x201))
    f << "fmtId=" << input->readULong(2) << ",";
  if (fl&0x10)
    f << "numLines=" << input->readULong(2) << ",";
  zone.closeFlagZone();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  SWFormatManager formatManager;
  if (input->peek()=='f') formatManager.readSWFormatDef(zone,'f',doc);
  if (input->peek()=='N') readSWContent(zone, doc);
  long lastPos=zone.getRecordLastPosition();
  while (input->tell()<lastPos) {
    pos=input->tell();
    if (readSWTableLine(zone, doc)) continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  zone.closeSWRecord('t', "SWTableBox");
  return true;
}

bool SDWParser::readSWTableLine(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='L' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3table.cxx: InTableLine
  libstoff::DebugStream f;
  f << "Entries(SWTableLine)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  if (fl&0x20 || !zone.isCompatibleWith(0x201))
    f << "fmtId=" << input->readULong(2) << ",";
  f << "nBoxes=" << input->readULong(2) << ",";
  zone.closeFlagZone();
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  SWFormatManager formatManager;
  if (input->peek()=='f')
    formatManager.readSWFormatDef(zone,'f',doc);

  long lastPos=zone.getRecordLastPosition();
  while (input->tell()<lastPos) {
    pos=input->tell();
    if (readSWTableBox(zone, doc))
      continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  zone.closeSWRecord('L', "SWTableLine");
  return true;
}

bool SDWParser::readSWTextZone(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='T' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3nodes.cxx: InTxtNode
  libstoff::DebugStream f;
  f << "Entries(SWText)[" << zone.getRecordLevel() << "]:";
  int fl=zone.openFlagZone();
  f << "nColl=" << input->readULong(2) << ",";
  int val;
  if (fl&0x10 && !zone.isCompatibleWith(0x201)) {
    val=(int) input->readULong(1);
    if (val==200 && zone.isCompatibleWith(0xf,0x101) && input->tell() < zone.getFlagLastPosition())
      val=(int) input->readULong(1);
    if (val)
      f << "nLevel=" << val << ",";
  }
  if (zone.isCompatibleWith(0x19,0x22, 0x101))
    f << "nCondColl=" << input->readULong(2) << ",";
  zone.closeFlagZone();

  librevenge::RVNGString text;
  if (!zone.readString(text)) {
    STOFF_DEBUG_MSG(("SDWParser::readSWTextZone: can not read main text\n"));
    f << "###text";
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord('T', "SWText");
    return true;
  }
  else if (!text.empty())
    f << text.cstr();

  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());

  long lastPos=zone.getRecordLastPosition();
  SWFormatManager formatManager;
  while (input->tell()<lastPos) {
    pos=input->tell();

    bool done=false;
    int rType=input->peek();

    switch (rType) {
    case 'A':
      done=readSWAttribute(zone, doc);
      break;
    case 'R':
      done=readSWNumRule(zone,'R');
      break;
    case 'S':
      done=readSWAttributeList(zone, doc);
      break;
    case 'l': // related to link
    case 'o': // format: safe to ignore
      done=formatManager.readSWFormatDef(zone,char(rType), doc);
      break;
    case 'v':
      done=readSWNodeRedline(zone);
      break;
    default:
      break;
    }
    if (done)
      continue;
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f.str("");
    f << "SWText[" << type << "-" << zone.getRecordLevel() << "]:";
    switch (type) {
    case '3': {
      // sw_sw3num InNodeNum
      f << "nodeNum,";
      int cFlag=zone.openFlagZone();
      int nLevel=(int) input->readULong(1);
      if (nLevel!=201)
        f << "nLevel=" << nLevel<< ",";
      if (cFlag&0x20) f << "nSetValue=" << input->readULong(2) << ",";
      zone.closeFlagZone();
      if (nLevel!=201) {
        int N=int(zone.getRecordLastPosition()-input->tell())/2;
        f << "level=[";
        for (int i=0; i<N; ++i)
          f << input->readULong(2) << ",";
        f << "],";
      }
      break;
    }
    case 'K':
      // sw_sw3misc.cxx InNodeMark
      f << "nodeMark,";
      f << "cType=" << input->readULong(1) << ",";
      f << "nId=" << input->readULong(2) << ",";
      f << "nOff=" << input->readULong(2) << ",";
      break;
    case 'w': { // wrong list
      // sw_sw3nodes.cxx in text node
      f << "wrongList,";
      int cFlag=zone.openFlagZone();
      if (cFlag&0xf0) f << "flag=" << (cFlag>>4) << ",";
      f << "nBeginInv=" << input->readULong(2) << ",";
      f << "nEndInc=" << input->readULong(2) << ",";
      zone.closeFlagZone();
      int N =(int) input->readULong(2);
      if (input->tell()+4*N>zone.getRecordLastPosition()) {
        STOFF_DEBUG_MSG(("SDWParser::readSWTextZone: find bad count\n"));
        f << "###N=" << N << ",";
        break;
      }
      f << "nWrong=[";
      for (int i=0; i<N; ++i) f << input->readULong(4) << ",";
      f << "],";
      break;
    }
    default:
      STOFF_DEBUG_MSG(("SDWParser::readSWTextZone: find unexpected type\n"));
      f << "###";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWText");
  }
  zone.closeSWRecord('T', "SWText");
  return true;
}

bool SDWParser::readSWTOXList(StarZone &zone, StarDocument &doc)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='u' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3misc.cxx: InTOXs
  libstoff::DebugStream f;
  f << "Entries(SWTOXList)[" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  long lastPos=zone.getRecordLastPosition();
  librevenge::RVNGString string;
  while (input->tell()<lastPos) {
    pos=input->tell();
    int rType=input->peek();
    if (rType!='x' || !zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    long lastRecordPos=zone.getRecordLastPosition();
    f << "SWTOXList:";
    int fl=zone.openFlagZone();
    if (fl&0xf0)
      f << "fl=" << (fl>>4) << ",";
    f << "nType=" << input->readULong(2) << ",";
    f << "nCreateType=" << input->readULong(2) << ",";
    f << "nCaptionDisplay=" << input->readULong(2) << ",";
    f << "nStrIdx=" << input->readULong(2) << ",";
    f << "nSeqStrIdx=" << input->readULong(2) << ",";
    f << "nData=" << input->readULong(2) << ",";
    f << "cFormFlags=" << input->readULong(1) << ",";
    zone.closeFlagZone();
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWTOXList: can not read aName\n"));
      f << "###aName";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOXList");
      continue;
    }
    if (!string.empty())
      f << "aName=" << string.cstr() << ",";
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWTOXList: can not read aTitle\n"));
      f << "###aTitle";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOXList");
      continue;
    }
    if (!string.empty())
      f << "aTitle=" << string.cstr() << ",";
    if (zone.isCompatibleWith(0x215)) {
      f << "nOLEOptions=" << input->readULong(2) << ",";
      f << "nMainStyleIdx=" << input->readULong(2) << ",";
    }
    else {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWTOXList: can not read aDummy\n"));
        f << "###aDummy";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord(type, "SWTOXList");
        continue;
      }
      if (!string.empty())
        f << "aDummy=" << string.cstr() << ",";
    }

    int N=(int) input->readULong(1);
    f << "nPatterns=" << N << ",";
    bool ok=true;
    SWFormatManager formatManager;
    for (int i=0; i<N; ++i) {
      if (formatManager.readSWPatternLCL(zone))
        continue;
      ok=false;
      f << "###pat";
      break;
    }
    if (!ok) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOXList");
      continue;
    }
    N=(int) input->readULong(1);
    f << "nTmpl=" << N << ",";
    f << "tmpl[strId]=[";
    for (int i=0; i<N; ++i)
      f << input->readULong(2) << ",";
    f << "],";
    N=(int) input->readULong(1);
    f << "nStyle=" << N << ",";
    f << "style=[";
    for (int i=0; i<N; ++i) {
      f << "[";
      f << "level=" << input->readULong(1) << ",";
      int nCount=(int) input->readULong(2);
      f << "nCount=" << nCount << ",";
      if (input->tell()+2*nCount>lastRecordPos) {
        STOFF_DEBUG_MSG(("SDWParser::readSWTOXList: can not read some string id\n"));
        f << "###styleId";
        ok=false;
        break;
      }
      librevenge::RVNGString poolName;
      for (int j=0; j<nCount; ++j) {
        int val=(int) input->readULong(2);
        if (!zone.getPoolName(val, poolName))
          f << "###nPoolId=" << val << ",";
        else
          f << poolName.cstr() << ",";
      }
      f << "],";
    }
    f << "],";
    if (!ok) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOXList");
      continue;
    }
    fl=zone.openFlagZone();
    f << "nSectStrIdx=" << input->readULong(2) << ",";
    if (fl&0x10) f << "nTitleLen=" << input->readULong(4) << ",";
    zone.closeFlagZone();

    if ((fl&0x10)) {
      while (input->tell()<zone.getRecordLastPosition() && input->peek()=='s') {
        if (!formatManager.readSWFormatDef(zone,'s', doc)) {
          STOFF_DEBUG_MSG(("SDWParser::readSWTOXList: can not read some format\n"));
          f << "###format,";
          break;
        }
      }
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWTOXList");
  }
  zone.closeSWRecord('u', "SWTOXList");
  return true;
}

bool SDWParser::readSWTOX51List(StarZone &zone)
{
  STOFFInputStreamPtr input=zone.input();
  libstoff::DebugFile &ascFile=zone.ascii();
  char type;
  long pos=input->tell();
  if (input->peek()!='y' || !zone.openSWRecord(type)) {
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    return false;
  }
  // sw_sw3misc.cxx: InTOX51s
  libstoff::DebugStream f;
  f << "Entries(SWTOX51List)[" << zone.getRecordLevel() << "]:";
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  long lastPos=zone.getRecordLastPosition();
  librevenge::RVNGString string;
  while (input->tell()<lastPos) {
    pos=input->tell();
    if (input->peek()!='x' || !zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    f << "SWTOX51List:";
    if (zone.isCompatibleWith(0x201)) {
      int strId=(int) input->readULong(2);
      if (strId!=0xFFFF && !zone.getPoolName(strId, string))
        f << "###nPoolId=" << strId << ",";
      else if (strId!=0xFFFF && !string.empty())
        f << string.cstr() << ",";
    }
    else {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWTOX51List: can not read typeName\n"));
        f << "###typeName";
        ascFile.addPos(pos);
        ascFile.addNote(f.str().c_str());
        zone.closeSWRecord(type, "SWTOX51List");
        continue;
      }
      if (!string.empty())
        f << "typeName=" << string.cstr() << ",";
    }
    if (!zone.readString(string)) {
      STOFF_DEBUG_MSG(("SDWParser::readSWTOX51List: can not read aTitle\n"));
      f << "###aTitle";
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOX51List");
      continue;
    }
    if (!string.empty())
      f << "aTitle=" << string.cstr() << ",";
    int fl=zone.openFlagZone();
    f << "nCreateType=" << input->readLong(2) << ",";
    f << "nType=" << input->readULong(1) << ",";
    if (zone.isCompatibleWith(0x213) && (fl&0x10))
      f << "firstTabPos=" << input->readULong(2) << ",";

    int N=(int) input->readULong(1);
    f << "nPat=" << N << ",";
    f << "pat=[";
    bool ok=true;
    for (int i=0; i<N; ++i) {
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readSWTOX51List: can not read a pattern name\n"));
        f << "###pat";
        ok=false;
        break;
      }
      if (!string.empty())
        f << string.cstr() << ",";
    }
    f << "],";
    if (!ok) {
      ascFile.addPos(pos);
      ascFile.addNote(f.str().c_str());
      zone.closeSWRecord(type, "SWTOX51List");
      continue;
    }
    N=(int) input->readULong(1);
    f << "nTmpl=" << N << ",";
    f << "tmpl[strId]=[";
    for (int i=0; i<N; ++i)
      f << input->readULong(2) << ",";
    f << "],";

    f << "nInf=" << input->readULong(2) << ",";

    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWTOX51List");
  }
  zone.closeSWRecord('y', "SWTOX51List");
  return true;
}

////////////////////////////////////////////////////////////
// drawing layer
////////////////////////////////////////////////////////////
bool SDWParser::readDrawingLayer(STOFFInputStreamPtr input, std::string const &name, StarDocument &document)
try
{
  StarZone zone(input, name, "DrawingLayer", document.getPassword());
  input->seek(0, librevenge::RVNG_SEEK_SET);
  libstoff::DebugFile &ascFile=zone.ascii();
  ascFile.open(name);
  // sw_sw3imp.cxx Sw3IoImp::LoadDrawingLayer

  // create this pool from the main's SWG pool
  shared_ptr<StarItemPool> pool=document.getNewItemPool(StarItemPool::T_XOutdevPool);
  pool->addSecondaryPool(document.getNewItemPool(StarItemPool::T_EditEnginePool));

  while (!input->isEnd()) {
    // REMOVEME: remove this loop, when creation of secondary pool is checked
    long pos=input->tell();
    bool extraPool=false;
    if (!pool) {
      extraPool=true;
      pool=document.getNewItemPool(StarItemPool::T_Unknown);
    }
    if (pool && pool->read(zone)) {
      if (extraPool) {
        STOFF_DEBUG_MSG(("SDWParser::readDrawingLayer: create extra pool for %d of type %d\n",
                         (int) document.getDocumentKind(), (int) pool->getType()));
      }
      pool.reset();
      continue;
    }
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    break;
  }
  long pos=input->tell();
  SDCParser sdcParser;
  if (!sdcParser.readSdrModel(zone, document)) {
    STOFF_DEBUG_MSG(("SDWParser::readDrawingLayer: can not find the drawing model\n"));
    input->seek(pos, librevenge::RVNG_SEEK_SET);
    ascFile.addPos(input->tell());
    ascFile.addNote("Entries(DrawingLayer):###extra");
    return true;
  }
  if (input->isEnd()) return true;
  pos=input->tell();
  uint16_t nSign;
  *input >> nSign;
  libstoff::DebugStream f;
  f << "Entries(DrawingLayer):";
  bool ok=true;
  if (nSign!=0x444D && nSign!=0) // 0 seems ok if followed by 0
    input->seek(pos, librevenge::RVNG_SEEK_SET);
  else {
    uint16_t n;
    *input >> n;
    if (pos+4+4*long(n)>input->size()) {
      STOFF_DEBUG_MSG(("SDWParser::readDrawingLayer: bad n frame\n"));
      f << "###pos";
      ok=false;
    }
    else {
      f << "framePos=[";
      for (uint16_t i=0; i<n; ++i) f << input->readULong(4) << ",";
      f << "],";
    }
  }
  if (ok && input->tell()+4==input->size())
    f << "num[hidden]=" << input->readULong(4) << ",";
  if (ok && !input->isEnd()) {
    STOFF_DEBUG_MSG(("SDWParser::readDrawingLayer: find extra data\n"));
    f << "###extra";
  }
  ascFile.addPos(pos);
  ascFile.addNote(f.str().c_str());
  return true;
}
catch (...)
{
  return false;
}

////////////////////////////////////////////////////////////
// main zone
////////////////////////////////////////////////////////////
bool SDWParser::readWriterDocument(STOFFInputStreamPtr input, std::string const &name, StarDocument &doc)
try
{
  StarZone zone(input, name, "SWWriterDocument", doc.getPassword());
  if (!zone.readSWHeader()) {
    STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: can not read the header\n"));
    return false;
  }
  libstoff::DebugFile &ascFile=zone.ascii();
  // sw_sw3doc.cxx Sw3IoImp::LoadDocContents
  SWFieldManager fieldManager;
  StarFileManager fileManager;
  SWFormatManager formatManager;
  while (!input->isEnd()) {
    long pos=input->tell();
    int rType=input->peek();
    bool done=false;
    switch (rType) {
    case '!':
      done=zone.readStringsPool();
      break;
    case 'R':
    case '0': // Outline
      done=readSWNumRule(zone,char(rType));
      break;
    case '1':
      done=readSWFootNoteInfo(zone);
      break;
    case '4':
      done=readSWEndNoteInfo(zone);
      break;
    case 'D':
      done=readSWDBName(zone);
      break;
    case 'F':
      done=formatManager.readSWFlyFrameList(zone, doc);
      break;
    case 'J':
      done=readSWJobSetUp(zone);
      break;
    case 'M':
      done=readSWMacroTable(zone);
      break;
    case 'N':
      done=readSWContent(zone, doc);
      break;
    case 'U': // layout info, no code, ignored by LibreOffice
      done=readSWLayoutInfo(zone);
      break;
    case 'V':
      done=readSWRedlineList(zone);
      break;
    case 'Y':
      done=fieldManager.readField(zone,'Y');
      break;

    case 'a':
      done=readSWBookmarkList(zone);
      break;
    case 'j':
      done=readSWDictionary(zone);
      break;
    case 'q':
      done=formatManager.readSWNumberFormatterList(zone);
      break;
    case 'u':
      done=readSWTOXList(zone, doc);
      break;
    case 'y':
      done=readSWTOX51List(zone);
      break;
    default:
      break;
    }
    if (done)
      continue;

    input->seek(pos, librevenge::RVNG_SEEK_SET);
    char type;
    if (!zone.openSWRecord(type)) {
      input->seek(pos, librevenge::RVNG_SEEK_SET);
      break;
    }
    libstoff::DebugStream f;
    f << "SWWriterDocument[" << type << "]:";
    long lastPos=zone.getRecordLastPosition();
    bool endZone=false;
    librevenge::RVNGString string;
    switch (type) {
    case '$': // unknown, seems to store an object name
      f << "dollarZone,";
      if (input->tell()+7>lastPos) {
        STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: zone seems to short\n"));
        break;
      }
      for (int i=0; i<5; ++i) { // f0=f1=1
        int val=(int) input->readULong(1);
        if (val) f << "f" << i << "=" << val << ",";
      }
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: can not read main string\n"));
        f << "###string";
        break;
      }
      else if (!string.empty())
        f << string.cstr();
      break;
    case '5': {
      // sw_sw3misc.cxx InLineNumberInfo
      int fl=zone.openFlagZone();
      if (fl&0xf0) f << "fl=" << (fl>>4) << ",";
      f << "linenumberInfo=" << input->readULong(1) << ",";
      f << "nPos=" << input->readULong(1) << ",";
      f << "nChrIdx=" << input->readULong(2) << ",";
      f << "nPosFromLeft=" << input->readULong(2) << ",";
      f << "nCountBy=" << input->readULong(2) << ",";
      f << "nDividerCountBy=" << input->readULong(2) << ",";
      zone.closeFlagZone();
      if (!zone.readString(string)) {
        STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: can not read sDivider string\n"));
        f << "###sDivider";
        break;
      }
      else if (!string.empty())
        f << string.cstr();
      break;
    }
    case '6':
      // sw_sw3misc.cxx InDocDummies
      f << "docDummies,";
      f << "n1=" << input->readULong(4) << ",";
      f << "n2=" << input->readULong(4) << ",";
      f << "n3=" << input->readULong(1) << ",";
      f << "n4=" << input->readULong(1) << ",";
      for (int i=0; i<2; ++i) {
        if (!zone.readString(string)) {
          STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: can not read a string\n"));
          f << "###string";
          break;
        }
        else if (!string.empty())
          f << (i==0 ? "sAutoMarkURL" : "s2") << "=" << string.cstr() << ",";
      }
      break;
    case '7': { // config, ignored by LibreOffice, and find no code
      f << "config,";
      int fl=(int) zone.openFlagZone();
      if (fl&0xf0) f << "fl=" << (fl>>4) << ",";
      f << "f0=" << input->readULong(1) << ","; // 1
      for (int i=0; i<5; ++i) // e,1,5,1,5
        f << "f" << i+1 << "=" << input->readULong(2) << ",";
      zone.closeFlagZone();
      break;
    }
    case '8':
      // sw_sw3misc.cxx InPagePreviewPrintData
      f << "pagePreviewPrintData,";
      f << "cFlags=" << input->readULong(1) << ",";
      f << "nRow=" << input->readULong(2) << ",";
      f << "nCol=" << input->readULong(2) << ",";
      f << "nLeftSpace=" << input->readULong(2) << ",";
      f << "nRightSpace=" << input->readULong(2) << ",";
      f << "nTopSpace=" << input->readULong(2) << ",";
      f << "nBottomSpace=" << input->readULong(2) << ",";
      f << "nHorzSpace=" << input->readULong(2) << ",";
      f << "nVertSpace=" << input->readULong(2) << ",";
      break;
    case 'd':
      // sw_sw3misc.cxx: InDocStat
      f << "docStats,";
      f << "nTbl=" << input->readULong(2) << ",";
      f << "nGraf=" << input->readULong(2) << ",";
      f << "nOLE=" << input->readULong(2) << ",";
      if (zone.isCompatibleWith(0x201)) {
        f << "nPage=" << input->readULong(4) << ",";
        f << "nPara=" << input->readULong(4) << ",";
      }
      else {
        f << "nPage=" << input->readULong(2) << ",";
        f << "nPara=" << input->readULong(2) << ",";
      }
      f << "nWord=" << input->readULong(4) << ",";
      f << "nChar=" << input->readULong(4) << ",";
      f << "nModified=" << input->readULong(1) << ",";
      break;
    case 'C': { // ignored by LibreOffice
      std::string comment("");
      while (lastPos && input->tell()<lastPos) comment+=(char) input->readULong(1);
      f << "comment=" << comment << ",";
      break;
    }
    case 'P': // password
      // sw_sw3misc.cxx: InPasswd
      f << "passwd,";
      if (zone.isCompatibleWith(0x6)) {
        f << "cType=" << input->readULong(1) << ",";
        if (!zone.readString(string)) {
          STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: can not read passwd string\n"));
          f << "###passwd";
        }
        else
          f << "cryptedPasswd=" << string.cstr() << ",";
      }
      break;
    case 'Z':
      endZone=true;
      break;
    default:
      STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: find unexpected type\n"));
      f << "###type,";
    }
    ascFile.addPos(pos);
    ascFile.addNote(f.str().c_str());
    zone.closeSWRecord(type, "SWWriterDocument");
    if (endZone)
      break;
  }
  if (!input->isEnd()) {
    STOFF_DEBUG_MSG(("SDWParser::readWriterDocument: find extra data\n"));
    ascFile.addPos(input->tell());
    ascFile.addNote("SWWriterDocument:##extra");
  }
  return true;
}
catch (...)
{
  return false;
}
////////////////////////////////////////////////////////////
//
// Low level
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// read the header
////////////////////////////////////////////////////////////
bool SDWParser::checkHeader(STOFFHeader *header, bool /*strict*/)
{
  *m_state = SDWParserInternal::State();

  STOFFInputStreamPtr input = getInput();
  if (!input || !input->hasDataFork() || !input->isStructured())
    return false;

  if (header)
    header->reset(1);

  return true;
}


// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:

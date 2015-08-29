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

/*
 * Parser to convert sdc StarOffice document
 *
 */
#ifndef SDC_PARSER
#  define SDC_PARSER

#include <vector>

#include "STOFFDebug.hxx"
#include "STOFFEntry.hxx"
#include "STOFFInputStream.hxx"

#include "STOFFParser.hxx"

namespace SDCParserInternal
{
struct State;
}

class StarAttribute;
class StarDocument;
class StarZone;

/** \brief the main class to read a StarOffice sdc file
 *
 *
 *
 */
class SDCParser
{
public:
  //! constructor
  SDCParser();
  //! destructor
  virtual ~SDCParser();

  //! try to read a chart zone: StarChartDocument .sds
  bool readChartDocument(STOFFInputStreamPtr input, std::string const &fileName, StarDocument &document);
  //! try to read a spreadshet style zone: SfxStyleSheets
  bool readSfxStyleSheets(STOFFInputStreamPtr input, std::string const &fileName, StarDocument &document);

  //! try to read a SdrModel zone: "DrMd"
  bool readSdrModel(StarZone &zone, StarDocument &doc);
protected:
  //
  // data
  //
  //! try to read the chart attributes
  bool readSCHAttributes(StarZone &zone, StarDocument &doc);
  //! try to read the memchart data
  bool readSCHMemChart(StarZone &zone);

  //! try to read a SdrLayer zone: "DrLy'
  bool readSdrLayer(StarZone &zone);
  //! try to read a SdrLayerSet zone: "DrLS'
  bool readSdrLayerSet(StarZone &zone);
  //! try to read a object zone: "DrOb'
  bool readSdrObject(StarZone &zone);
  //! try to read a Page/MasterPage zone: "DrPg'
  bool readSdrPage(StarZone &zone, StarDocument &doc);
  //! try to read a Master Page descriptor zone: "DrMP'
  bool readSdrMPageDesc(StarZone &zone);
  //! try to read a list of Master Page zone: "DrML'
  bool readSdrMPageDescList(StarZone &zone);

  //! the state
  shared_ptr<SDCParserInternal::State> m_state;
};
#endif
// vim: set filetype=cpp tabstop=2 shiftwidth=2 cindent autoindent smartindent noexpandtab:

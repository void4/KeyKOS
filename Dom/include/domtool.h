/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************
  Domain Tool Defines
  KC (DomTool,DomTool_MakeDomainKey) KEYSFROM(Node) KEYSTO(domain)
  KC (DomTool,DomTool_IdentifyStart) KEYSFROM(start,brand)
       KEYSTO(Node)
  KC (DomTool,DomTool_IdentifyResume) KEYSFROM(resume,brand)
       KEYSTO(Node)
  KC (DomTool,DomTool_IdentifyDomain) KEYSFROM(Domain,brand)
       KEYSTO(Node)
  KC (DomTool,DomTool_IdentifySegment) KEYSFROM(segment,brand)
       KEYSTO(Node)
  KC (DomTool,DomTool_IdentifySegmentWithResumeKeyKeeper)
       KEYSFROM(segment,brand) KEYSTO(Node)
  KC (DomTool,DomTool_IdentifySegmentWithDomainKeyKeeper)
       KEYSFROM(segment,brand) KEYSTO(Node)
  KC (DomTool,DomTool_IdentifyMeter) KEYSFROM(Meter,brand)
       KEYSTO(Node)
  KC (DomTool,DomTool_IdentifyMeterWithResumeKeyKeeper)
       KEYSFROM(Meter,brand) KEYSTO(Node)
  KC (DomTool,DomTool_IdentifyMeterWithDomainKeyKeeper)
       KEYSFROM(Meter,brand) KEYSTO(Node)
 
***************************************************************/
#ifndef _H_domtool
#define _H_domtool

#define DomTool_AKT                              0x109
 
#define DomTool_MakeDomainKey                        0
 
#define DomTool_IdentifyStart                        1
#define DomTool_IdentifyResume                       2
#define DomTool_IdentifyDomain                       3
 
#define DomTool_IdentifySegment                      5
#define DomTool_IdentifySegmentWithResumeKeyKeeper   6
#define DomTool_IdentifySegmentWithDomainKeyKeeper   7
 
#define DomTool_IdentifyMeter                        9
#define DomTool_IdentifyMeterWithResumeKeyKeeper    10
#define DomTool_IdentifyMeterWithDomainKeyKeeper    11
 
#endif

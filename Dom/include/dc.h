/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************
  Domain Creator Defines
 
  KC (DC,DC_DestroyDomain) KEYSFROM(domain,sb)
  KC (DC,DC_IdentifyStart) KEYSFROM(start) KEYSTO(domain)
  KC (DC,DC_IdentifyResume) KEYSFROM(resume) KEYSTO(domain)
  KC (DC,DC_IdentifySegment) KEYSFROM(segment) KEYSTO(domain,node)
  KC (DC,DC_IdentifySegmentWithResumeKeyKeeper) KEYSFROM(segment)
                             KEYSTO(domain,node)
  KC (DC,DC_IdentifySegmentWithDomainKeyKeeper) KEYSFROM(segment)
                             KEYSTO(domain,node)
  KC (DC,DC_DestroyMe) STRUCTFROM(DC_ReturnCode)
                             KEYSFROM(returnkey,sb)
  KC (DC,DC_IdentifyMeter) KEYSFROM(meter) KEYSTO(domain,node)
  KC (DC,DC_IdentifyMeterWithResumeKeyKeeper) KEYSFROM(meter)
                             KEYSTO(domain,node)
  KC (DC,DC_IdentifyMeterWithDomainKeyKeeper) KEYSFROM(meter)
                             KEYSTO(domain,node)
  KC (DC,DC_CreateDomain) KEYSFROM(sb) KEYSTO(domain)
  KC (DC,DC_Weaken) KEYSTO(DCR)
  KC (DC,DC_SeverDomain) KEYSFROM(start,sb)
                             KEYSTO(domain)
  KC (DC,DC_SeverMe) KEYSFROM(sb) KEYSTO(domain)
 
***************************************************************/

#ifndef _H_dc
#define _H_dc
 
#define DC_AKT                               0x0D
 
#define DC_DestroyDomain                        1
#define DC_IdentifyStart                        2
#define DC_IdentifyResume                       3
#define DC_IdentifySegment                      5
#define DC_IdentifySegmentWithResumeKeyKeeper   6
#define DC_IdentifySegmentWithDomainKeyKeeper   7
#define DC_DestroyMe                            8
#define DC_IdentifyMeter                        9
#define DC_IdentifyMeterWithResumeKeyKeeper    10
#define DC_IdentifyMeterWithDomainKeyKeeper    11
#define DC_CreateDomain                        12
#define DC_Weaken                              13
#define DC_SeverDomain                         14
#define DC_SeverMe                             15
 
#endif

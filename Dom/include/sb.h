/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

/***************************************************************
  Definitions for SpaceBank (SB)
 
  SB(SB_CreateNode) KEYSTO(node)
  SB(SB_DestroyNode) KEYSFROM(node)
  SB(SB_SeverNode) KEYSFROM(node)
  SB(SB_QueryNodeSpace) STRUCTO(SB_Values)
  SB(SB_QueryNodeStatistics) STRUCTRO(SB_Statistics)
  SB(SB_CreateTwoNodes) KEYSTO(key1,key2)
  SB(SB_CreateThreeNodes) KEYSTO(key1,key2,key3)
  SB(SB_DestroyTwoNodes) KEYSFROM(key1,key2)
  SB(SB_DestroyThreeNodes) KEYSFROM(key1,key2,key3)
  SB(SB_ChangeSmallNodeLimit) STRUCTFROM (SB_SmallChangeValue)
                          STRUCTTO (SB_SmallChangeValue)
  SB(SB_SetNodeRangeLimit) STRUCTFROM (SB_Limits)
  SB(SB_ChangeNodeLimit) STRUCTFROM (SB_ChangeValue)
                          STRUCTTO (SB_ChangeValue)
  SB(SB_QueryNodesAvailable) STRUCTTO (SB_ChangeValue)
 
  SB(SB_CreatePage) KEYSTO(page)
  SB(SB_DestroyPage) KEYSFROM(page)
  SB(SB_SeverPage) KEYSFROM(page)
  SB(SB_QueryPageSpace) STRUCTRO(SB_Values)
  SB(SB_QueryPageStatistics) STRUCTTO(SB_Statistics)
  SB(SB_CreateTwoPages) KEYSTO(key1,key2)
  SB(SB_CreateThreePages) KEYSTO(key1,key2,key3)
  SB(SB_DestroyTwoPages) KEYSFROM(key1,key2)
  SB(SB_DestroyThreePages) KEYSFROM(key1,key2,key3)
  SB(SB_ChangeSmallPageLimit) STRUCTFROM (SB_SmallChangeValue)
                          STRUCTTO (SB_SmallChangeValue)
  SB(SB_SetPageRangeLimit) STRUCTFROM (SB_Limits)
  SB(SB_ChangePageLimit) STRUCTFROM (SB_ChangeValue)
                          STRUCTTO (SB_ChangeValue)
  SB(SB_QueryPagesAvailable) STRUCTTO (SB_ChangeValue)
 
  SB(SB_ForbidDestroy) KEYSTO (newbank);
  SB(SB_ForbidQuery) KEYSTO (newbank);
  SB(SB_ForbidDestroyAndQuery) KEYSTO (newbank);
  SB(SB_ForbidChangeLimits) KEYSTO (newbank);
  SB(SB_ForbidDestroyAndChangeLimits) KEYSTO (newbank);
  SB(SB_ForbidQueryAndChangeLimits) KEYSTO (newbank);
  SB(SB_ForbidDestroyQueryAndChangeLimits) KEYSTO (newbank);
 
  SB(SB_DestroyBankAndSpace);
  SB(SB_QueryStatistics) STRUCTTO (SB_FullStatistics);
  SB(SB_VerifyBank) KEYSFROM (sb1) RCTO (rc);
  SB(SB_CreateBank) KEYSTO (newbank);
 
***************************************************************/
#ifndef _H_sb
#define _H_sb

#include <lli.h>
#include <kktypes.h>
 
#define SB_AKT                     0x0C
 
#define SB_CreateNode                 0
#define SB_DestroyNode                1
#define SB_SeverNode                  2
#define SB_QueryNodeSpace             5
#define SB_QueryNodeStatistics        6
#define SB_CreateTwoNodes             7
#define SB_CreateThreeNodes           8
#define SB_DestroyTwoNodes            9
#define SB_DestroyThreeNodes         10
#define SB_ChangeSmallNodeLimit      11
#define SB_SetNodeRangeLimit         12
#define SB_ChangeNodeLimit           13
#define SB_QueryNodesAvailable       14
#define SB_CreatePage                16
#define SB_DestroyPage               17
#define SB_SeverPage                 18
#define SB_QueryPageSpace            21
#define SB_QueryPageStatistics       22
#define SB_CreateTwoPages            23
#define SB_CreateThreePages          24
#define SB_DestroyTwoPages           25
#define SB_DestroyThreePages         26
#define SB_ChangeSmallPageLimit      27
#define SB_SetPageRangeLimit         28
#define SB_ChangePageLimit           29
#define SB_QueryPagesAvailable       30
 
#define SB_ForbidDestroy                     33
#define SB_ForbidQuery                       34
#define SB_ForbidDestroyAndQuery             35
#define SB_ForbidChangeLimits                36
#define SB_ForbidDestroyAndChangeLimits      37
#define SB_ForbidQueryAndChangeLimits        38
#define SB_ForbidDestroyQueryAndChangeLimits 39
 
#define SB_DestroyBankAndSpace               64
#define SB_QueryStatistics                   65
#define SB_VerifyBank                        66
#define SB_CreateBank                        67
 
  struct SB_Limits {
    LLI Lower;
    LLI Upper;
  };
 
  struct SB_Values {
    UINT32 Quantity;
  };
 
  struct SB_Statistics {
    UINT32 Creates;
    UINT32 Destroys;
  };
 
  struct SB_SmallChangeValue {
    SINT32 Delta;
  };
 
  struct SB_ChangeValue {
    LLI Delta;
  };
 
  struct SB_FullStatistics {
    LLI NodeCreates;
    LLI NodeDestroys;
    LLI PageCreates;
    LLI PageDestroys;
  };

  struct SB_FullStatisticsLL {
    long long NodeCreates;
    long long NodeDestroys;
    long long PageCreates;
    long long PageDestroys;
  };
 
#endif

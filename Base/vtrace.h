/* Copyright (c) 2005 Agorics; see MIT_License in this directory
or http://www.opensource.org/licenses/mit-license.html */

#define TRACE_0(fac, tag, name)
#define TRACE_1(fac, tag, name, d1)
#define TRACE_2(fac, tag, name, d1, d2)
#define TRACE_3(fac, tag, name, d1, d2, d3)
#define TRACE_4(fac, tag, name, d1, d2, d3, d4)
#define TRACE_5(fac, tag, name, d1, d2, d3, d4, d5)

#define TR_FAC_SCSI             21      /* SCSI */
#define TR_FAC_SCSI_RES         38      /* SCSI_RESOURCE */
#define TR_FAC_SCSI_ISP         39      /* ISP HBA Driver SCSI */

/*
 * TR_FAC_SCSI tags
 */
 
#define TR_ESPSVC_ACTION_CALL                   0
 
#define TR_ESPSVC_START                         1
#define TR_ESPSVC_END                           2
#define TR_ESP_CALLBACK_START                   3
#define TR_ESP_CALLBACK_END                     4
#define TR_ESP_DOPOLL_START                     5
#define TR_ESP_DOPOLL_END                       6
#define TR_ESP_FINISH_START                     7
#define TR_ESP_FINISH_END                       8
#define TR_ESP_FINISH_SELECT_START              9
#define TR_ESP_FINISH_SELECT_RESET1_END         10
#define TR_ESP_FINISH_SELECT_RETURN1_END        11
#define TR_ESP_FINISH_SELECT_RETURN2_END        12
#define TR_ESP_FINISH_SELECT_FINISH_END         13
#define TR_ESP_FINISH_SELECT_ACTION1_END        14
#define TR_ESP_FINISH_SELECT_ACTION2_END        15
#define TR_ESP_FINISH_SELECT_RESET2_END         16
#define TR_ESP_FINISH_SELECT_RESET3_END         17
#define TR_ESP_FINISH_SELECT_ACTION3_END        18
#define TR_ESP_HANDLE_CLEARING_START            19
#define TR_ESP_HANDLE_CLEARING_END              20
#define TR_ESP_HANDLE_CLEARING_FINRST_END       21
#define TR_ESP_HANDLE_CLEARING_RETURN1_END      22
 
#define TR_ESP_HANDLE_CLEARING_ABORT_END        23
#define TR_ESP_HANDLE_CLEARING_LINKED_CMD_END   24
#define TR_ESP_HANDLE_CLEARING_RETURN2_END      25
#define TR_ESP_HANDLE_CLEARING_RETURN3_END      26
#define TR_ESP_HANDLE_CMD_START_START           27
#define TR_ESP_HANDLE_CMD_START_END             28
#define TR_ESP_HANDLE_CMD_START_ABORT_CMD_END   29
#define TR_ESP_HANDLE_CMD_DONE_START            30
#define TR_ESP_HANDLE_CMD_DONE_END              31
#define TR_ESP_HANDLE_CMD_DONE_ABORT1_END       32
#define TR_ESP_HANDLE_CMD_DONE_ABORT2_END       33
#define TR_ESP_HANDLE_C_CMPLT_START             34
#define TR_ESP_HANDLE_C_CMPLT_FINRST_END        35
#define TR_ESP_HANDLE_C_CMPLT_RETURN1_END       36
#define TR_ESP_HANDLE_C_CMPLT_ACTION1_END       37
#define TR_ESP_HANDLE_C_CMPLT_ACTION2_END       38
#define TR_ESP_HANDLE_C_CMPLT_ACTION3_END       39
#define TR_ESP_HANDLE_C_CMPLT_ACTION4_END       40
#define TR_ESP_HANDLE_C_CMPLT_RETURN2_END       41
#define TR_ESP_HANDLE_C_CMPLT_ACTION5_END       42
#define TR_ESP_HANDLE_C_CMPLT_PHASEMANAGE_END   43
#define TR_ESP_HANDLE_DATA_START                44
#define TR_ESP_HANDLE_DATA_END                  45
#define TR_ESP_HANDLE_DATA_ABORT1_END           46
#define TR_ESP_HANDLE_DATA_ABORT2_END           47
#define TR_ESP_HANDLE_DATA_ABORT3_END           48
#define TR_ESP_HANDLE_DATA_DONE_START           49
#define TR_ESP_HANDLE_DATA_DONE_END             50
#define TR_ESP_HANDLE_DATA_DONE_RESET_END       51
#define TR_ESP_HANDLE_DATA_DONE_PHASEMANAGE_END 52
#define TR_ESP_HANDLE_DATA_DONE_ACTION1_END     53
#define TR_ESP_HANDLE_DATA_DONE_ACTION2_END     54
#define TR_ESP_HANDLE_MORE_MSGIN_START          55
#define TR_ESP_HANDLE_MORE_MSGIN_RETURN1_END    56
#define TR_ESP_HANDLE_MORE_MSGIN_RETURN2_END    57
#define TR_ESP_HANDLE_MSG_IN_START              58
#define TR_ESP_HANDLE_MSG_IN_END                59
#define TR_ESP_HANDLE_MSG_IN_DONE_START         60
#define TR_ESP_HANDLE_MSG_IN_DONE_FINRST_END    61
#define TR_ESP_HANDLE_MSG_IN_DONE_RETURN1_END   62
#define TR_ESP_HANDLE_MSG_IN_DONE_PHASEMANAGE_END       63
#define TR_ESP_HANDLE_MSG_IN_DONE_SNDMSG_END    64
#define TR_ESP_HANDLE_MSG_IN_DONE_ACTION_END    65
#define TR_ESP_HANDLE_MSG_IN_DONE_RETURN2_END   66
#define TR_ESP_HANDLE_MSG_OUT_START             67
#define TR_ESP_HANDLE_MSG_OUT_END               68
#define TR_ESP_HANDLE_MSG_OUT_PHASEMANAGE_END   69
#define TR_ESP_HANDLE_MSG_OUT_DONE_START        70
#define TR_ESP_HANDLE_MSG_OUT_DONE_END          71
#define TR_ESP_HANDLE_MSG_OUT_DONE_FINISH_END   72
#define TR_ESP_HANDLE_MSG_OUT_DONE_PHASEMANAGE_END      73
#define TR_ESP_HANDLE_SELECTION_START           74
#define TR_ESP_HANDLE_SELECTION_END             75
#define TR_ESP_HANDLE_UNKNOWN_START             76
#define TR_ESP_HANDLE_UNKNOWN_END               77
#define TR_ESP_HANDLE_UNKNOWN_INT_DISCON_END    78
#define TR_ESP_HANDLE_UNKNOWN_PHASE_DATA_END    79
#define TR_ESP_HANDLE_UNKNOWN_PHASE_MSG_OUT_END 80
#define TR_ESP_HANDLE_UNKNOWN_PHASE_MSG_IN_END  81
#define TR_ESP_HANDLE_UNKNOWN_PHASE_STATUS_END  82
#define TR_ESP_HANDLE_UNKNOWN_PHASE_CMD_END     83
#define TR_ESP_HANDLE_UNKNOWN_RESET_END         84
#define TR_ESP_HDATAD_START                     85
#define TR_ESP_HDATAD_END                       86
#define TR_ESP_HDATA_START                      87
#define TR_ESP_HDATA_END                        88
#define TR_ESP_ISTART_START                     89
#define TR_ESP_ISTART_END                       90
 
#define TR_ESP_PHASEMANAGE_CALL                 91
 
#define TR_ESP_PHASEMANAGE_START                92
#define TR_ESP_PHASEMANAGE_END                  93
#define TR_ESP_POLL_START                       94
#define TR_ESP_POLL_END                         95
#define TR_ESP_POLL_END                         95
#define TR_ESP_RECONNECT_START                  96
#define TR_ESP_RECONNECT_F1_END                 97
#define TR_ESP_RECONNECT_RETURN1_END            98
#define TR_ESP_RECONNECT_F2_END                 99
#define TR_ESP_RECONNECT_PHASEMANAGE_END        100
#define TR_ESP_RECONNECT_F3_END                 101
#define TR_ESP_RECONNECT_RESET1_END             102
#define TR_ESP_RECONNECT_RESET2_END             103
#define TR_ESP_RECONNECT_RESET3_END             104
#define TR_ESP_RECONNECT_SEARCH_END             105
#define TR_ESP_RECONNECT_RESET4_END             106
#define TR_ESP_RECONNECT_RETURN2_END            107
#define TR_ESP_RECONNECT_RESET5_END             108
#define TR_ESP_RUNPOLL_START                    109
#define TR_ESP_RUNPOLL_END                      110
#define TR_ESP_SCSI_IMPL_PKTALLOC_START         111
#define TR_ESP_SCSI_IMPL_PKTALLOC_END           112
#define TR_ESP_SCSI_IMPL_PKTFREE_START          113
#define TR_ESP_SCSI_IMPL_PKTFREE_END            114
#define TR_ESP_STARTCMD_START                   115
#define TR_ESP_STARTCMD_END                     116
#define TR_ESP_STARTCMD_RE_SELECTION_END        117
#define TR_ESP_STARTCMD_ALLOC_TAG1_END          118
#define TR_ESP_STARTCMD_ALLOC_TAG2_END          119
 
#define TR_ESP_STARTCMD_PREEMPT_CALL            120
 
#define TR_ESP_START_START                      121
#define TR_ESP_START_END                        122
#define TR_ESP_START_PREPARE_PKT_END            123
#define TR_ESP_WATCH_START                      124
#define TR_ESP_WATCH_END                        125



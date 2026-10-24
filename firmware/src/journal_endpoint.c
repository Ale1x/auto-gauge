#include "ag_journal_endpoint.h"
#include <string.h>

ag_device_history_result_t ag_journal_endpoint_read(void *context,const uint8_t sid[16],
                                                  uint64_t after,ag_history_snapshot_t *out) {
    if(out==NULL)return AG_DEVICE_HISTORY_UNAVAILABLE;
    memset(out,0,sizeof(*out));
    if(context==NULL)return AG_DEVICE_HISTORY_UNAVAILABLE;
    ag_journal_result_t result=ag_journal_window(context,sid,after,out);
    switch(result) {
    case AG_JOURNAL_OK:return AG_DEVICE_HISTORY_OK;
    case AG_JOURNAL_READ_ONLY:return AG_DEVICE_HISTORY_READ_ONLY;
    case AG_JOURNAL_NOT_RETAINED:return AG_DEVICE_HISTORY_NOT_RETAINED;
    case AG_JOURNAL_CURSOR_AHEAD:return AG_DEVICE_HISTORY_CURSOR_AHEAD;
    default:return AG_DEVICE_HISTORY_UNAVAILABLE;
    }
}

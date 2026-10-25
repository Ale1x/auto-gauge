#ifndef AG_JOURNAL_ENDPOINT_H
#define AG_JOURNAL_ENDPOINT_H
#include "ag_device_endpoint.h"
#include "ag_journal_history.h"

/* Direct options.history_read adapter; context points to an opened journal.
 * Read/query only: never bootstraps, appends, repairs, erases or consumes ACKs.
 * One owner task, same lifetime/serialization contract as journal and endpoint.
 * Failure clears output; no previous client's snapshot can be reused as success.
 */
ag_device_history_result_t ag_journal_endpoint_read(void *,const uint8_t sid[16],
                                                  uint64_t after,ag_history_snapshot_t *);
ag_device_history_result_t ag_journal_endpoint_sessions(void *,const uint8_t after_sid_or_null[16],
    uint8_t limit,ag_history_sessions_page_t *);
#endif

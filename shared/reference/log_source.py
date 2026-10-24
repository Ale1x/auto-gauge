"""Volatile bounded history source; client ACK never erases shared records."""
from collections import deque
import json
from log_import import MAX_PAGE_BYTES, MAX_PAGE_RECORDS, MAX_SEQUENCE, uint_string


class LogPageSource:
    def __init__(self,session_id,capacity=64,first_sequence=1):
        if not isinstance(session_id,str) or len(session_id)!=32 or any(c not in '0123456789abcdef' for c in session_id) or int(session_id,16)==0:
            raise ValueError('Invalid session')
        if type(capacity) is not int or not 1<=capacity<=64:raise ValueError('Invalid development capacity')
        if type(first_sequence) is not int or not 1<=first_sequence<=MAX_SEQUENCE:raise ValueError('Invalid starting sequence')
        self.session_id=session_id
        self.records=deque(maxlen=capacity)
        # Recovery/test input, never a remote command. A real store must derive
        # this value from verified persistent metadata, not guess lost history.
        self.next_sequence=first_sequence
        self.last_ms=0

    def append(self,pid,ecu_source,monotonic_ms,status,raw):
        if type(pid) is not int or not 1<=pid<=255 or type(ecu_source) is not int or not 0<=ecu_source<=0x1fffffff:
            raise ValueError('Invalid PID/source')
        if type(monotonic_ms) is not int or not self.last_ms<=monotonic_ms<=MAX_SEQUENCE:raise ValueError('Invalid monotonic time')
        if status not in ('valid','unsupported','unavailable','stale','timeout','error'):raise ValueError('Invalid status')
        if not isinstance(raw,bytes) or len(raw)>64 or (status=='valid')!=(len(raw)>0):raise ValueError('Invalid raw/status')
        if self.next_sequence>MAX_SEQUENCE:raise ValueError('Sequence exhausted')
        self.records.append(dict(sequence=str(self.next_sequence),monotonic_ms=str(monotonic_ms),
                                 pid=pid,ecu_source=ecu_source,status=status,raw_hex=raw.hex()))
        self.next_sequence+=1
        self.last_ms=monotonic_ms

    def read_page(self,after_sequence,limit=MAX_PAGE_RECORDS,maximum_bytes=MAX_PAGE_BYTES):
        cursor=uint_string(after_sequence,True)
        if type(limit) is not int or not 1<=limit<=MAX_PAGE_RECORDS:raise ValueError('Invalid page limit')
        if type(maximum_bytes) is not int or not 512<=maximum_bytes<=MAX_PAGE_BYTES:raise ValueError('Invalid page byte budget')
        if cursor>=self.next_sequence:raise ValueError('Cursor is ahead of this session')
        first_available=int(self.records[0]['sequence']) if self.records else self.next_sequence
        lost=[]
        if cursor+1<first_available:lost=[dict(first=str(cursor+1),last=str(first_available-1))]
        result=dict(session_id=self.session_id,records=[],lost=lost)
        # Work is bounded by the fixed ring capacity, independent of N clients.
        for record in self.records:
            if int(record['sequence'])<=cursor:continue
            candidate=dict(result,records=[*result['records'],record])
            if len(json.dumps(candidate,separators=(',',':')).encode())>maximum_bytes:break
            result=candidate
            if len(result['records'])==limit:break
        # Return value objects; client mutation must never change the source.
        return json.loads(json.dumps(result,separators=(',',':')))

    def acknowledge(self,session_id,through):
        cursor=uint_string(through,True)
        if session_id!=self.session_id or cursor>=self.next_sequence:raise ValueError('Invalid ACK')
        # Flow-progress signal only. No deletion and no permanent per-client map.
        return cursor

"""Transactional local reference archive, with explicit gap reporting and resume."""
import json
import sqlite3

MAX_PAGE_RECORDS=16
MAX_PAGE_BYTES=4096
MAX_SEQUENCE=(1<<64)-1


def uint_string(value,allow_zero=False):
    if not isinstance(value,str) or not 1<=len(value)<=20 or not value.isascii() or not value.isdigit():raise ValueError('Invalid uint string')
    parsed=int(value)
    if str(parsed)!=value or not (0 if allow_zero else 1)<=parsed<=MAX_SEQUENCE:raise ValueError('Invalid uint range')
    return parsed


class LogArchive:
    """ACK is produced only after a SQLite transaction commits locally."""
    def __init__(self,path):
        self.db=sqlite3.connect(path)
        self.db.execute('CREATE TABLE IF NOT EXISTS records(device TEXT, session TEXT, seq TEXT, body TEXT, PRIMARY KEY(device,session,seq))')
        self.db.execute('CREATE TABLE IF NOT EXISTS cursors(device TEXT, session TEXT, seq TEXT, PRIMARY KEY(device,session))')
        self.db.execute('CREATE TABLE IF NOT EXISTS losses(device TEXT, session TEXT, first TEXT, last TEXT, PRIMARY KEY(device,session,first,last))')

    def close(self):self.db.close()

    def cursor(self,device,session):
        row=self.db.execute('SELECT seq FROM cursors WHERE device=? AND session=?',(device,session)).fetchone()
        return int(row[0]) if row else 0

    def import_page(self,device,page):
        # Validate bounded primitive fields before serializing caller objects.
        if not isinstance(device,str) or not 1<=len(device)<=64:raise ValueError('Invalid device')
        if not isinstance(page,dict) or not 2<=len(page)<=3 or set(page)-{'session_id','records','lost'}:raise ValueError('Invalid page')
        session=page.get('session_id')
        if not isinstance(session,str) or len(session)!=32 or any(c not in '0123456789abcdef' for c in session) or int(session,16)==0:
            raise ValueError('Invalid session')
        records=page.get('records'); lost=page.get('lost',[])
        if not isinstance(records,list) or len(records)>MAX_PAGE_RECORDS or not isinstance(lost,list) or len(lost)>16:
            raise ValueError('Unbounded page')
        intervals=[]
        for interval in lost:
            if not isinstance(interval,dict) or len(interval)!=2 or set(interval)!={'first','last'}:raise ValueError('Invalid gap')
            a,b=uint_string(interval.get('first')),uint_string(interval.get('last'))
            if b<a:raise ValueError('Invalid gap ordering')
            intervals.append((a,b))
        intervals.sort()
        if any(a<=previous_b for (_,previous_b),(a,_) in zip(intervals,intervals[1:])):raise ValueError('Overlapping gaps')
        parsed=[]; previous=0
        for record in records:
            if not isinstance(record,dict) or len(record)!=6 or set(record)!={'sequence','monotonic_ms','pid','ecu_source','status','raw_hex'}:raise ValueError('Invalid record')
            seq=uint_string(record.get('sequence'))
            uint_string(record.get('monotonic_ms'),True)
            pid=record.get('pid'); ecu=record.get('ecu_source'); status=record.get('status'); raw=record.get('raw_hex')
            if type(pid) is not int or not 1<=pid<=255 or type(ecu) is not int or not 0<=ecu<=0x1fffffff:raise ValueError('Invalid PID/source')
            if status not in ('valid','unsupported','unavailable','stale','timeout','error'):raise ValueError('Invalid status')
            if not isinstance(raw,str) or len(raw)>128 or len(raw)%2 or any(c not in '0123456789abcdef' for c in raw):raise ValueError('Invalid raw')
            if (status=='valid')!=(len(raw)>0):raise ValueError('Raw/status mismatch')
            if seq<=previous or any(a<=seq<=b for a,b in intervals):raise ValueError('Duplicate/unordered/lost record')
            previous=seq
            parsed.append((seq,json.dumps(record,sort_keys=True,separators=(',',':'))))
        # No implicit jump over missing sequence numbers. Overwrite is a loss,
        # represented by exact intervals, not the firmware session envelope.
        encoded=json.dumps(page,separators=(',',':'),allow_nan=False).encode()
        if len(encoded)>MAX_PAGE_BYTES:raise ValueError('Page too large')
        with self.db:
            # Serialize cursor read/check/write across separate importers too.
            # ACK must never move backwards after another connection commits.
            self.db.execute('BEGIN IMMEDIATE')
            current=self.cursor(device,session); cursor=current
            gap_intervals=list(intervals)
            for seq,_ in parsed:
                if seq>cursor:intervals.append((seq,seq))
            for a,b in sorted(intervals):
                if a>cursor+1:raise ValueError('Unexplained sequence gap')
                cursor=max(cursor,b)
            for a,b in gap_intervals:
                if self.db.execute('SELECT 1 FROM records WHERE device=? AND session=? AND seq BETWEEN ? AND ? LIMIT 1',
                                   (device,session,f'{a:020d}',f'{b:020d}')).fetchone():
                    raise ValueError('Loss interval contradicts imported record')
                self.db.execute('INSERT OR IGNORE INTO losses VALUES(?,?,?,?)',(device,session,f'{a:020d}',f'{b:020d}'))
            for seq,body in parsed:
                key=f'{seq:020d}'
                old=self.db.execute('SELECT body FROM records WHERE device=? AND session=? AND seq=?',(device,session,key)).fetchone()
                if old:
                    if old[0]!=body:raise ValueError('Duplicate record has conflicting content')
                else:
                    if seq<=current:raise ValueError('Late record conflicts with acknowledged loss')
                    self.db.execute('INSERT INTO records VALUES(?,?,?,?)',(device,session,key,body))
            self.db.execute('INSERT INTO cursors VALUES(?,?,?) ON CONFLICT(device,session) DO UPDATE SET seq=excluded.seq',(device,session,str(cursor)))
        return dict(session_id=session,acknowledged_through=str(cursor),reported_loss=lost)

"""Local reference endpoint. No sockets, BLE, device, cloud or authentication."""
import json
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'scripts'))
from check_shared_contract import validate
from framing import Frame, decode, encode
from log_source import LogPageSource


def reject_duplicate_keys(pairs):
    value={}
    for key,item in pairs:
        if key in value: raise ValueError('Duplicate JSON key')
        value[key]=item
    return value


def normalized_configuration(config):
    """Project schema1 fields, like Swift Codable; never retain extensions."""
    return dict(schema_version=config['schema_version'],revision=config['revision'],
                brightness=dict(mode=config['brightness']['mode'],manual_percent=config['brightness']['manual_percent']),
                dashboards=[dict(id=d['id'],widgets=[dict(id=w['id'],pid=w['pid'],
                    rect={key:w['rect'][key] for key in ('x','y','width','height')})
                    for w in d['widgets']]) for d in config['dashboards']])


class DeviceSimulator:
    """Config changes affect only this explicitly simulated in-memory object."""
    def __init__(self,configuration,log_source=None):
        if not validate(configuration):raise ValueError('Invalid initial configuration')
        self.configuration=normalized_configuration(configuration)
        self.supported={4,5,12,13,15,16,17}
        if log_source is not None and not isinstance(log_source,LogPageSource):raise ValueError('Invalid history source')
        self.log_source=log_source

    def handle(self,packet):
        frame=decode(packet)
        if frame.kind!=1:raise ValueError('Only requests are accepted')
        def reply(value):
            payload=json.dumps(dict(simulation=True,**value),separators=(',',':'),ensure_ascii=False,allow_nan=False).encode()
            return encode(Frame(2,frame.request_id,payload))
        try:
            obj=json.loads(frame.payload.decode('utf-8'),object_pairs_hook=reject_duplicate_keys,
                           parse_constant=lambda _: (_ for _ in ()).throw(ValueError('Nonfinite JSON')))
            if not isinstance(obj,dict) or not isinstance(obj.get('op'),str):raise ValueError('Invalid object')
        except (ValueError,UnicodeError,RecursionError):
            return [reply(dict(phase='rejected',error='invalid_message'))]
        operation=obj['op']
        if operation=='hello':
            return [reply(dict(op='hello',device_id='SIMULATO-001',protocol_version=1,
                               supported_service01_pids=sorted(self.supported),authenticated=False))]
        if operation=='config.get':
            return [reply(dict(phase='current',configuration=self.configuration,persistence='volatile'))]
        if operation in ('logs.read','logs.ack'):
            if self.log_source is None:return [reply(dict(phase='rejected',error='history_unavailable'))]
            if obj.get('session_id')!=self.log_source.session_id:
                return [reply(dict(phase='rejected',error='session_mismatch'))]
            try:
                if operation=='logs.read':
                    # Reserve the exact envelope length in the framing budget.
                    overhead=len(json.dumps(dict(simulation=True,op='logs.read',page={}),separators=(',',':')).encode())-2
                    page=self.log_source.read_page(obj.get('after_sequence'),obj.get('limit',16),4096-overhead)
                    return [reply(dict(op='logs.read',page=page))]
                through=self.log_source.acknowledge(obj['session_id'],obj.get('acknowledged_through'))
                return [reply(dict(op='logs.ack',session_id=obj['session_id'],acknowledged_through=str(through),source_records_deleted=False))]
            except ValueError:return [reply(dict(phase='rejected',error='invalid_history_request'))]
        if operation in ('config.validate','config.apply'):
            responses=[]
            if operation=='config.apply':responses.append(reply(dict(phase='received')))
            config=obj.get('configuration')
            if not validate(config):
                return responses+[reply(dict(phase='rejected',error='invalid_configuration'))]
            if any(w['pid'] not in self.supported for d in config['dashboards'] for w in d['widgets']):
                return responses+[reply(dict(phase='rejected',error='unsupported_pid'))]
            if operation=='config.validate':return [reply(dict(phase='validated'))]
            expected=obj.get('expected_revision')
            if type(expected) is not int or expected!=self.configuration['revision'] or config['revision']!=expected+1:
                return responses+[reply(dict(phase='rejected',error='revision_conflict'))]
            candidate=normalized_configuration(config)
            # Confirm the complete state can be read back before accepting it.
            reply(dict(phase='current',configuration=candidate,persistence='volatile'))
            self.configuration=candidate
            return responses+[reply(dict(phase='applied',applied_revision=config['revision'],persistence='volatile'))]
        return [reply(dict(phase='rejected',error='not_implemented'))]

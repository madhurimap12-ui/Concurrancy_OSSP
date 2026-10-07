#!/usr/bin/env python3
import base64, json, os, signal, subprocess, threading, time, urllib.parse, urllib.request, secrets
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]; RUNTIME=ROOT/'runtime'; RES=ROOT/'resources'; LOG=ROOT/'logs/events.jsonl'; PID=RUNTIME/'server.pid'; METRICS=RUNTIME/'metrics.json'; CATALOG=RUNTIME/'resources.json'; DASH_PID=RUNTIME/'dashboard.pid'
PORT=8080; SERVER_PORT=9090
server_proc=None; lock=threading.Lock(); sessions={}
# Demo-only local credentials for the academic project presentation.
USERS={'admin':{'password':'admin123','role':'administrator'},'student':{'password':'student123','role':'student'}}
_cpu_prev=None; _cpu_lock=threading.Lock()

def read_json(path, default):
    try: return json.loads(path.read_text())
    except Exception: return default

def write_json(path,data): path.write_text(json.dumps(data,indent=2))

def resources(): return read_json(CATALOG,[])
def save_resources(items): write_json(CATALOG,items)

def sync_catalog():
    items=resources(); changed=False
    for x in items:
        p=RES/x['filename']
        if p.exists():
            size=p.stat().st_size; updated=time.strftime('%Y-%m-%d %H:%M:%S',time.localtime(p.stat().st_mtime))
            if x.get('size')!=size or x.get('updated')!=updated: x['size']=size; x['updated']=updated; changed=True
    if changed: save_resources(items)

def server_running():
    global server_proc
    if server_proc and server_proc.poll() is None: return True
    if PID.exists():
        try: os.kill(int(PID.read_text()),0); return True
        except Exception: pass
    return False

def start_server(workers=8):
    global server_proc
    if server_running(): return
    subprocess.run([str(ROOT/'scripts/build.sh')],check=True)
    server_proc=subprocess.Popen([str(ROOT/'server/portal_server'),str(max(1,min(int(workers),64))),str(SERVER_PORT)],cwd=ROOT)
    time.sleep(.4)

def stop_server():
    global server_proc
    if PID.exists():
        try: os.kill(int(PID.read_text()),signal.SIGTERM)
        except Exception: pass
    elif server_proc: server_proc.terminate()

def metrics():
    m=read_json(METRICS,{}); m['python_time']=time.time(); m['server_running']=server_running(); return m

def cpu_percent():
    global _cpu_prev
    try:
        line=Path('/proc/stat').read_text().splitlines()[0].split()
        vals=list(map(int,line[1:])); idle=vals[3]+(vals[4] if len(vals)>4 else 0); total=sum(vals)
        with _cpu_lock:
            prev=_cpu_prev; _cpu_prev=(total,idle)
        if not prev: return 0.0
        dt=total-prev[0]; di=idle-prev[1]
        return max(0.0,min(100.0,(1-(di/dt if dt else 0))*100))
    except Exception: return 0.0

def process_telemetry():
    try:
        pid=int(PID.read_text()) if PID.exists() else 0
        threads=0; rss=0
        if pid and Path(f'/proc/{pid}/status').exists():
            for line in Path(f'/proc/{pid}/status').read_text().splitlines():
                if line.startswith('Threads:'): threads=int(line.split()[1])
                if line.startswith('VmRSS:'): rss=int(line.split()[1])*1024
        return {'pid':pid,'threads':threads,'rss':rss}
    except Exception: return {'pid':0,'threads':0,'rss':0}

def memory():
    info={}
    try:
        for line in Path('/proc/meminfo').read_text().splitlines():
            k,v=line.split(':',1); parts=v.strip().split(); info[k]=int(parts[0])*(1024 if len(parts)>1 and parts[1]=='kB' else 1)
        total=info.get('MemTotal',0); avail=info.get('MemAvailable',info.get('MemFree',0)); used=max(0,total-avail)
        return {'total':total,'available':avail,'used':used,'used_percent':(used/total*100 if total else 0),'free':info.get('MemFree',0),'buffers':info.get('Buffers',0),'cached':info.get('Cached',0),'cpu_percent':cpu_percent(),'process':process_telemetry()}
    except Exception as e: return {'error':str(e)}

def event_tail(n=100):
    if not LOG.exists(): return []
    lines=LOG.read_text(errors='ignore').splitlines()[-n:]; out=[]
    for l in lines:
        try: out.append(json.loads(l))
        except Exception: pass
    return out

def api(path,method,body,role=None):
    if path=='/api/me': return {'authenticated':bool(role),'role':role}
    if path=='/api/status': return {'metrics':metrics(),'memory':memory(),'events':event_tail(100),'resources':resources(),'role':role}
    if path=='/api/resources' and method=='GET': return resources()
    if role!='administrator' and path in ('/api/start','/api/stop','/api/fault','/api/performance'): return {'error':'Administrator access required'},403
    if path=='/api/resources' and method=='POST':
        data=body; rid='r-'+str(int(time.time()*1000)); filename=Path(data.get('filename') or (rid+'.txt')).name
        if not filename: filename=rid+'.txt'
        content=data.get('content','').encode();
        if data.get('base64'): content=base64.b64decode(data['content'])
        (RES/filename).write_bytes(content)
        item={'id':rid,'title':data.get('title') or filename,'category':data.get('category') or 'Other','description':data.get('description',''),'filename':filename,'size':len(content),'updated':time.strftime('%Y-%m-%d %H:%M:%S')}
        items=resources(); items.append(item); save_resources(items); return item
    if path.startswith('/api/resources/'):
        rid=path.rsplit('/',1)[-1]; items=resources(); item=next((x for x in items if x['id']==rid),None)
        if not item: return {'error':'resource not found'},404
        if method=='PUT':
            data=body; old=item['filename']
            for k in ('title','category','description'):
                if k in data: item[k]=data[k]
            if data.get('filename') or 'content' in data:
                filename=Path(data.get('filename') or old).name; content=data.get('content','').encode()
                if data.get('base64'): content=base64.b64decode(data['content'])
                (RES/filename).write_bytes(content)
                if filename!=old and (RES/old).exists(): (RES/old).unlink()
                item['filename']=filename; item['size']=len(content); item['updated']=time.strftime('%Y-%m-%d %H:%M:%S')
            save_resources(items); return item
        if method=='DELETE':
            p=RES/item['filename'];
            if p.exists(): p.unlink()
            items=[x for x in items if x['id']!=rid]; save_resources(items); return {'deleted':rid}
    if path=='/api/start' and method=='POST': start_server(body.get('workers',8)); return {'ok':True,'running':server_running()}
    if path=='/api/stop' and method=='POST': stop_server(); return {'ok':True}
    if path=='/api/fault' and method=='POST':
        if not server_running(): start_server()
        os.kill(int(PID.read_text()),signal.SIGUSR1); return {'ok':True,'message':'worker fault requested'}
    if path=='/api/performance' and method=='POST':
        count=max(1,min(int(body.get('requests',50)),1000)); url=f'http://127.0.0.1:{SERVER_PORT}/health'; start=time.perf_counter(); ok=0; errors=0
        def one(_):
            nonlocal ok,errors
            try:
                with urllib.request.urlopen(url,timeout=5) as r:
                    if r.status==200: ok += 1
                    else: errors += 1
            except Exception: errors += 1
        import concurrent.futures
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(count,64)) as ex: list(ex.map(one,range(count)))
        elapsed=time.perf_counter()-start
        return {'requests':count,'success':ok,'errors':errors,'elapsed':elapsed,'requests_per_second':ok/elapsed if elapsed else 0,'metrics':metrics()}
    return {'error':'unknown endpoint'},404

class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args): pass
    def _send(self,status,data,ctype='application/json',extra=None):
        if isinstance(data,(dict,list)): raw=json.dumps(data).encode(); ctype='application/json'
        elif isinstance(data,bytes): raw=data
        else: raw=data.encode()
        self.send_response(status); self.send_header('Content-Type',ctype); self.send_header('Content-Length',str(len(raw)))
        self.send_header('Access-Control-Allow-Origin','*'); self.send_header('Access-Control-Allow-Methods','GET,POST,PUT,DELETE,OPTIONS')
        if extra:
            for k,v in extra.items(): self.send_header(k,v)
        self.end_headers(); self.wfile.write(raw)
    def do_OPTIONS(self): self._send(204,b'', 'text/plain')
    def auth_role(self):
        raw=self.headers.get('Cookie',''); token=''
        for part in raw.split(';'):
            if part.strip().startswith('session='): token=part.strip().split('=',1)[1]
        return sessions.get(token)
    def do_GET(self):
        p=urllib.parse.urlparse(self.path).path
        if p.startswith('/api/'):
            role=self.auth_role()
            if p not in ('/api/me','/api/resources') and not role: self._send(401,{'error':'Login required'}); return
            result=api(p,'GET',{},role)
            if isinstance(result,tuple): self._send(result[1],result[0]); return
            self._send(200,result); return
        if p=='/' or p=='/index.html': self._send(200,(ROOT/'dashboard/index.html').read_bytes(),'text/html; charset=utf-8'); return
        if p.startswith('/static/'):
            fp=ROOT/'dashboard'/p.removeprefix('/static/')
            if fp.exists(): self._send(200,fp.read_bytes(),'text/javascript' if fp.suffix=='.js' else 'text/css'); return
        self._send(404,{'error':'not found'})
    def body(self):
        n=int(self.headers.get('Content-Length','0')); raw=self.rfile.read(n); return json.loads(raw or b'{}')
    def do_POST(self): self.dispatch('POST')
    def do_PUT(self): self.dispatch('PUT')
    def do_DELETE(self): self.dispatch('DELETE')
    def dispatch(self,method):
        p=urllib.parse.urlparse(self.path).path
        try: body=self.body()
        except Exception: body={}
        if p=='/api/login' and method=='POST':
            user=USERS.get(str(body.get('username','')).lower());
            if not user or body.get('password')!=user['password']: self._send(401,{'error':'Invalid username or password'}); return
            token=secrets.token_urlsafe(24); sessions[token]=user['role']; self._send(200,{'ok':True,'role':user['role']},extra={'Set-Cookie':f'session={token}; Path=/; HttpOnly; SameSite=Lax'}); return
        if p=='/api/logout' and method=='POST':
            raw=self.headers.get('Cookie',''); token=''
            for part in raw.split(';'):
                if part.strip().startswith('session='): token=part.strip().split('=',1)[1]
            sessions.pop(token,None); self._send(200,{'ok':True},extra={'Set-Cookie':'session=; Max-Age=0; Path=/'}); return
        role=self.auth_role()
        if not role: self._send(401,{'error':'Login required'}); return
        if p.startswith('/api/resources') and method in ('POST','PUT','DELETE') and role!='administrator': self._send(403,{'error':'Administrator access required'}); return
        if p.startswith('/api/') and p not in ('/api/status','/api/resources','/api/performance','/api/start','/api/stop','/api/fault'):
            self._send(404,{'error':'not found'}); return
        try: result=api(p,method,body,role)
        except Exception as e: result=({'error':str(e)},500)
        if isinstance(result,tuple): self._send(result[1],result[0])
        else: self._send(200,result)

if __name__=='__main__':
    RUNTIME.mkdir(exist_ok=True); RES.mkdir(exist_ok=True); LOG.parent.mkdir(exist_ok=True); DASH_PID.write_text(str(os.getpid())); sync_catalog(); start_server()
    try: ThreadingHTTPServer(('127.0.0.1',PORT),Handler).serve_forever()
    finally: stop_server(); DASH_PID.unlink(missing_ok=True)

#!/usr/bin/env python3
"""G09: compile with a real installed ESP8266 toolchain; never upload firmware."""
import argparse,datetime,hashlib,json,pathlib,shutil,subprocess,tempfile,sys
root=pathlib.Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--cli',default='arduino-cli')
ap.add_argument('--config-file',type=pathlib.Path)
ap.add_argument('--fqbn',default='esp8266:esp8266:nodemcuv2')
ap.add_argument('--objdump',type=pathlib.Path)
ap.add_argument('--build-dir',type=pathlib.Path)
ap.add_argument('--report',type=pathlib.Path,default=root/'docs/target-build-results.json')
args=ap.parse_args()
result={'timestampUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'firmwareSha256':hashlib.sha256((root/'CoffeeMachine/CoffeeMachine.ino').read_bytes()).hexdigest(),
        'fqbn':args.fqbn,'uploadPerformed':False,'status':'NOT_RUN'}
def save(code):
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ['status','fqbn','uploadPerformed']},ensure_ascii=False))
    sys.exit(code)
cli=shutil.which(args.cli)
if not cli:
    result['reason']='arduino-cli unavailable'; save(77)
base=[cli]+(['--config-file',str(args.config_file)] if args.config_file else [])
def run(*cmd):
    p=subprocess.run(base+list(cmd),capture_output=True,text=True)
    return {'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
result['toolchain']={k:run(*cmd) for k,cmd in [
    ('cli',('version',)),('cores',('core','list')),('libraries',('lib','list'))]}
build=args.build_dir or pathlib.Path(tempfile.mkdtemp(prefix='coffee-target-build-'))
build.mkdir(parents=True,exist_ok=True)
result['buildDirectory']=str(build)
result['compile']=run('compile','--fqbn',args.fqbn,'--warnings','all',
                      '--build-path',str(build),str(root/'CoffeeMachine'))
if result['compile']['exit']!=0:
    result['status']='FAIL'; result['reason']='ESP8266 compile/link failed'; save(1)
elfs=list(build.glob('*.elf'))
if not elfs:
    result['status']='FAIL'; result['reason']='compiled ELF missing'; save(1)
elf=elfs[0]; result['elfSha256']=hashlib.sha256(elf.read_bytes()).hexdigest()
if not args.objdump or not args.objdump.exists():
    result['status']='PARTIAL'; result['reason']='compiled; IRAM symbol check requires --objdump'; save(77)
p=subprocess.run([str(args.objdump),'-t',str(elf)],capture_output=True,text=True)
symbols=[line for line in p.stdout.splitlines() if 'FlowSensor' in line and 'flowIsr' in line]
# Linked ESP8266 IRAM symbols use addresses beginning with 0x401; flash irom uses 0x402.
iram=bool(symbols) and all(line.split()[0].lower().startswith('401') for line in symbols)
result['isrSymbolCheck']={'exit':p.returncode,'symbols':symbols,'iramAddress':iram}
result['status']='PASS' if p.returncode==0 and iram else 'FAIL'
if result['status']=='FAIL': result['reason']='ISR symbol not verified in linked IRAM'
save(0 if result['status']=='PASS' else 1)

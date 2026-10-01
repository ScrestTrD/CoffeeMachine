#!/usr/bin/env python3
"""Build assertions in a disposable directory; no firmware upload or device I/O."""
import argparse,json,pathlib,subprocess,tempfile,hashlib,datetime,sys
root=pathlib.Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser()
ap.add_argument('--baseline',type=pathlib.Path,help='Optional v6 snapshot: expect regression failure.')
ap.add_argument('--firmware',type=pathlib.Path,default=root/'CoffeeMachine/CoffeeMachine.ino',help='Source override for same-suite reproduction; no device I/O.')
ap.add_argument('--report',type=pathlib.Path,help='Save JSON verification evidence.')
args=ap.parse_args()
results={}
with tempfile.TemporaryDirectory(prefix='coffee-gates-') as tmp:
    for mode,extra in [('normal',[]),('sanitized',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
        binary=pathlib.Path(tmp)/mode
        command=['g++','-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-g']+extra+[
          '-DCOFFEE_FIRMWARE="'+str(args.firmware.resolve())+'"',
          '-I',str(root/'test/host_review'),str(root/'test/host_review/review.cpp'),'-o',str(binary)]
        build=subprocess.run(command,capture_output=True,text=True)
        item={'compileExit':build.returncode,'compileStderr':build.stderr}
        if build.returncode==0:
            run=subprocess.run([str(binary)],capture_output=True,text=True)
            item.update(runExit=run.returncode,stdout=run.stdout,stderr=run.stderr)
            print(run.stdout,end=''); print(run.stderr,end='',file=sys.stderr)
        else: print(build.stderr,file=sys.stderr)
        results[mode]=item
    if args.baseline:
        binary=pathlib.Path(tmp)/'baseline'
        cmd=['g++','-std=c++11','-DCOFFEE_LEGACY_BASELINE=1',
             '-DCOFFEE_FIRMWARE="'+str(args.baseline.resolve())+'"',
             '-I',str(root/'test/host_review'),str(root/'test/host_review/review.cpp'),'-o',str(binary)]
        build=subprocess.run(cmd,capture_output=True,text=True)
        item={'compileExit':build.returncode,'compileStderr':build.stderr}
        if build.returncode==0:
            run=subprocess.run([str(binary)],capture_output=True,text=True)
            item.update(runExit=run.returncode,stdout=run.stdout,stderr=run.stderr)
            print('BASELINE (expected failure):\n'+run.stdout,end='')
        results['baseline']=item
evidence={'timestampUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'firmwareSha256':hashlib.sha256(args.firmware.read_bytes()).hexdigest(),
          'platform':'host mocks; ESP8266 and bench gates NOT RUN','results':results}
if args.report:
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(evidence,indent=2,ensure_ascii=False)+'\n')
ok=all(results[k].get('compileExit')==0 and results[k].get('runExit')==0 for k in ['normal','sanitized'])
if args.baseline: ok=ok and results['baseline'].get('compileExit')==0 and results['baseline'].get('runExit')==1
sys.exit(0 if ok else 1)

from pathlib import Path
import argparse,json,re,xml.etree.ElementTree as E
parser=argparse.ArgumentParser()
parser.add_argument('--before',type=Path,required=True)
parser.add_argument('--after',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--allow-added-case')
args=parser.parse_args()
def cases(path):
    result={}
    for match in re.finditer(r'-{20,}\n(.*?)\n-{20,}\n[^\n]*:\d+\n\.{20,}\n(.*?)(?=\n-{20,}\n|\n={20,}|\Z)',path.read_text(),re.S):
        name=' '.join(match[1].split())
        values=[]
        for expansion in re.finditer(r'with expansion:\n(.*?)(?=\nwith messages?:|\n\n|\Z)',match[2],re.S):
            values.append(re.sub(r'0x[0-9a-fA-F]+','<ADDRESS>',' '.join(expansion[1].split())))
        result[name]=values
    return result
before=cases(args.before);after=cases(args.after);common=before.keys()&after.keys()
differences=[]
for name in sorted(common):
    if before[name]!=after[name]:
        changed=[{'index':i,'before':x,'after':y} for i,(x,y) in enumerate(zip(before[name],after[name])) if x!=y]
        differences.append({'name':name,'before_count':len(before[name]),'after_count':len(after[name]),'values':changed[:10]})
added=sorted(after.keys()-before.keys());missing=sorted(before.keys()-after.keys())
passed=bool(common) and not differences and not missing and added==([args.allow_added_case] if args.allow_added_case else [])
record={'result':'PASS' if passed else 'FAIL','before':str(args.before),'after':str(args.after),
        'scope':'Catch console expansions as printed; address tokens alone normalized; not a bitwise oracle for unprinted bounds',
        'before_cases':len(before),'after_cases':len(after),'compared_cases':len(common),
        'compared_expansions':sum(len(before[name]) for name in common),'differences':differences,'added':added,'missing':missing}
args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps(record));raise SystemExit(0 if passed else 1)

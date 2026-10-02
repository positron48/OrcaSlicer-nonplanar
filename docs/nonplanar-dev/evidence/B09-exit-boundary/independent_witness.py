"""70-digit independent diagnostic point refutation. Never grants route PASS."""
from pathlib import Path
from decimal import Decimal as D, localcontext
import copy, json, struct

def check(v):
    def number(s): return D(struct.unpack('>d',bytes.fromhex(s))[0])
    assert v['version']==1 and v['scope']=='declared_upper_point_refutation_only'
    assert v['status']=='FAIL' and v['reason']=='MATERIAL_MOTION_UPPER_INTERSECTION'
    c=json.loads(v['context']);motion=json.loads(v['motion'])['motion'];row=json.loads(v['material']);m=row['motion'];b=row['bead']
    assert motion[1]==v['event_index'] and m[1]==v['material_record'] and 0<=m[1]<=motion[1]<c['record_count']
    assert b and m[7][0]==1 and D(0)<=D(v['parameter'])<=D(1)
    with localcontext() as ctx:
        ctx.prec=70
        parameter=D(v['parameter']);local=list(map(D,v['local_point']));centre=list(map(D,v['tip_center']))
        radius2=sum((local[i]-centre[i])**2 for i in (0,1))
        assert local[2]==centre[2] and D(v['opening_radius'])**2<radius2<D(v['outer_radius'])**2
        pose=[number(motion[3][i])+parameter*(number(motion[4][i])-number(motion[3][i]))+local[i] for i in range(3)]
        start=list(map(number,m[3]));end=list(map(number,m[4]));dx=end[0]-start[0];dy=end[1]-start[1];length2=dx*dx+dy*dy;length=length2.sqrt()
        t=(dx*(pose[0]-start[0])+dy*(pose[1]-start[1]))/length2
        normal=abs((dx*(pose[1]-start[1])-dy*(pose[0]-start[0]))/length)
        progress=D(1) if m[1]<motion[1] else parameter
        xy=number(c['model'][1])+number(c['model'][5]);ze=number(c['model'][2])+number(c['model'][5])
        assert progress>0 and -xy/length<t<progress+xy/length
        local_t=max(D(0),min(progress,t));dh=number(b[2])-number(b[1]);dz=end[2]-start[2]
        h=number(b[1])+dh*local_t;top=start[2]+dz*local_t;hmin=min(number(b[1]),number(b[1])+dh*progress)
        area=number(m[7][1])/length;shift=min(xy/length,progress)
        assert hmin>0 and area>0 and b[0]==1
        pi=D('3.1415926535897932384626433832795028841971693993751058209749445923078164')
        core=area/(2*h)-pi*h/8+(area/hmin**2+pi/4)*abs(dh)/2*shift
        radius=h/2+xy+ze+(abs(dz-dh/2)+abs(dh)/2)*shift
        separation=radius**2-(max(normal-core,D(0))**2+(pose[2]-(top-h/2))**2)
        assert core>=0 and radius>0 and separation>0
        return {'material_record':m[1],'event_index':motion[1],'parameter':str(parameter),'radial_squared':str(radius2),'Upper_radius_squared_minus_distance_squared':str(separation),'result':'STRICT_UPPER_INTERSECTION_NOT_PHYSICAL_QUALIFICATION'}

root=Path(__file__).parent
v=json.loads((root/'final-ctest-native/native-exit-witness.json').read_text())
positive=check(v);refusals=[]
for change in ('opening','outside_ring','wrong_time','future','no_bead','move_z'):
    p=copy.deepcopy(v)
    if change=='opening':p['local_point']=p['tip_center']
    if change=='outside_ring':p['local_point']=[1,1,0]
    if change=='wrong_time':p['parameter']=1
    if change=='future':p['material_record']=p['event_index']+1
    if change=='no_bead':r=json.loads(p['material']);r['bead']=None;p['material']=json.dumps(r)
    if change=='move_z':r=json.loads(p['motion']);r['motion'][3][2]=struct.pack('>d',100).hex();p['motion']=json.dumps(r)
    try:check(p)
    except (AssertionError,TypeError):refusals.append(change)
    else:raise AssertionError('accepted mutation '+change)
result={'scope':'70_digit_independent_point_diagnostic_not_full_route','positive':positive,'refusals':refusals}
assert len(refusals)==6
(root/'independent-witness-result.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

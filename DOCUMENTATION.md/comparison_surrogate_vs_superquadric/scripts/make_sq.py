# Generate SQ inputs for every surrogate tilted case (03, 04, 05a/b/c) and blockiness sweep
import math,re,os,sys
SUR='/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/test_cases/surrogate_contact/'
SQT=open('/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/test_cases/superquadric_reference/sq_05c_cube_1cm_spin_h010.in').read()
cases=['test_03_cube_1cm_corner','test_04_cube_1cm_edge','test_05a_cube_1cm_spin_h002','test_05b_cube_1cm_spin_h005','test_05c_cube_1cm_spin_h010']
def params(f):
    s=open(SUR+f+'.in').read()
    h=re.search(r'create_atoms\s+1 single 0.0 0.0 (\S+)',s).group(1)
    q=re.search(r'^set\s+atom \* quat_direct (\S+) (\S+) (\S+) (\S+)',s,re.M).groups()
    w=re.search(r'omegax (\S+) omegay (\S+) omegaz (\S+)',s).groups()
    return h,q,w
out=sys.argv[1]; ns=[int(x) for x in sys.argv[2].split(',')]; dense=sys.argv[3]=='dense'
for c in cases:
    h,q,w=params(c)
    for n in ns:
        t=SQT
        t=re.sub(r'create_atoms    1 single 0.0 0.0 \S+','create_atoms    1 single 0.0 0.0 '+h,t)
        t=re.sub(r'blockiness \d+ \d+','blockiness %d %d'%(n,n),t)
        t=re.sub(r'^set             atom \* quat_direct .*$','set             atom * quat_direct %s %s %s %s'%q,t,flags=re.M)
        t=re.sub(r'omegax \S+ omegay \S+ omegaz \S+','omegax %s omegay %s omegaz %s'%w,t)
        if dense:
            t=re.sub(r'custom/vtk \d+ post','custom/vtk 1 post',t); t=t.replace('thermo          100','thermo          1')
        d=f'{out}/{c}/sq_n{n}'; os.makedirs(d,exist_ok=True); open(d+'/in','w').write(t)
    # surrogate dense copy
    s=open(SUR+c+'.in').read()
    if dense:
        s=re.sub(r'custom/vtk \d+ post','custom/vtk 1 post',s); s=s.replace('thermo          100','thermo          1')
    d=f'{out}/{c}/surrogate'; os.makedirs(d,exist_ok=True); open(d+'/in','w').write(s)
print('ok')

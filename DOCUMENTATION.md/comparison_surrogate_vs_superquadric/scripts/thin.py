import glob,os
D='/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/DOCUMENTATION.md/comparison_surrogate_vs_superquadric/data'
for f in glob.glob(D+'/ts_*.dat'):
    L=open(f).read().split('\n'); hdr=[l for l in L if l.startswith('#')]; rows=[l for l in L if l and not l.startswith('#')]
    E=[float(r.split()[14]) for r in rows]; E0=E[0]
    act=set(i for i in range(1,len(E)) if abs(E[i]-E[i-1])>1e-11*abs(E0))
    keep=set(range(0,len(rows),10))
    for i in act:
        keep.update(range(max(0,i-200),min(len(rows),i+201)))
    with open(f,'w') as o:
        o.write('\n'.join(hdr)+'\n# sampling: every step within +-200 steps of any contact, else every 10th step\n')
        for i in sorted(keep): o.write(rows[i]+'\n')
print('ok')

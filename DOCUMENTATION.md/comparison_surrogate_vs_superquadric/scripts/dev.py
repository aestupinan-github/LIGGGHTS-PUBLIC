import math
OUT='/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/DOCUMENTATION.md/comparison_surrogate_vs_superquadric/data'
rows=[l.split() for l in open(OUT+'/first_impact_summary.dat') if l.strip() and not l.startswith('#')]
by={}
for r in rows: by.setdefault(r[0],{})[r[1]]=r
o=open(OUT+'/deviation_vs_sq.dat','w')
o.write('# Deviation of each SQ blockiness from the surrogate (first impact), per case\n')
o.write('# columns: case blockiness corner_round_L |dvz_post| (m/s) |dw_post| (rad/s, vector norm) rel_dw (|dw|/|w_surr|) |d_onset| (steps)\n')
for c,d in by.items():
    s=d['surrogate']; ws=[float(x) for x in s[9:12]]; wsm=math.sqrt(sum(v*v for v in ws)) or 1.0
    for m in ['sq_n10','sq_n20','sq_n50','sq_n100','sq_n200','sq_n400','sq_n1000']:
        if m not in d: continue
        q=d[m]; wq=[float(x) for x in q[9:12]]
        dw=math.sqrt(sum((a-b)**2 for a,b in zip(ws,wq)))
        o.write(f'{c} {q[2]} {q[3]} {abs(float(q[8])-float(s[8])):.6f} {dw:.4f} {dw/wsm if wsm>1 else 0:.5f} {abs(int(q[4])-int(s[4]))}\n')
    o.write('\n\n')
o.close()
for l in open(OUT+'/deviation_vs_sq.dat'):
    if not l.startswith('#') and l.strip(): print(l.rstrip())

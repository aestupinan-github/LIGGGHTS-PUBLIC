import os,math,glob
B='/tmp/opencode/cmpw/dense'
OUT='/home/su_estupinan@private.list.lu/SpaceLights/LIGGGHTS-PUBLIC/DOCUMENTATION.md/comparison_surrogate_vs_superquadric/data'
L=0.01
cases=['test_03_cube_1cm_corner','test_04_cube_1cm_edge','test_05a_cube_1cm_spin_h002','test_05b_cube_1cm_spin_h005','test_05c_cube_1cm_spin_h010']
short={'test_03_cube_1cm_corner':'c03_corner','test_04_cube_1cm_edge':'c04_edge','test_05a_cube_1cm_spin_h002':'c05a_spin_h002','test_05b_cube_1cm_spin_h005':'c05b_spin_h005','test_05c_cube_1cm_spin_h010':'c05c_spin_h010'}
models=['surrogate']+['sq_n%d'%n for n in (10,20,50,100,200,400,1000)]
def load(f):
    rows=[]
    for l in open(f):
        p=l.split()
        if len(p)==15 and p[0].isdigit():
            try: rows.append([int(p[0])]+[float(x) for x in p[1:]])
            except: pass
    return rows
def corner_round(n): return 0.0 if n is None else 0.5*math.sqrt(3)*(1-3**(-1/n))
def impacts(r):
    # contact steps: |fz + m g| > tiny  (contact force present)
    E0=r[0][13]
    on=[i for i in range(1,len(r)) if abs(r[i][13]-r[i-1][13])>1e-11*abs(E0)]
    blocks=[]
    for i in on:
        if blocks and i-blocks[-1][1]<=3: blocks[-1][1]=i
        else: blocks.append([i,i])
    return blocks
summ=open(OUT+'/first_impact_summary.dat','w')
summ.write('# First-impact metrics, surrogate vs superquadric (SQ) cube, L = 0.01 m\n')
summ.write('# model: surrogate (sharp cube) or sq_nN (superquadric blockiness N); corner_round_L = corner rounding / L\n')
summ.write('# contact window = consecutive steps in which total energy changes (contact active); pre/post = 2 steps before / 1 after\n')
summ.write('# columns: case model blockiness corner_round_L onset_step n_steps zc_onset_L vz_pre vz_post wx_post wy_post wz_post wmag_post ke_post erot_post etot_post etot_loss_pct max_etot_over_E0 rebound_ratio\n')
for c in cases:
    for m in models:
        f=f'{B}/{c}/{m}/out.log'; r=load(f)
        if not r: print('EMPTY',f); continue
        n=None if m=='surrogate' else int(m.split('n')[1])
        # time series
        with open(f'{OUT}/ts_{short[c]}_{m}.dat','w') as o:
            o.write(f'# time series {c} / {m}; 1 row per step (dt = 1e-5 s)\n')
            o.write('# columns: step time_s xc_m yc_m zc_m vx vy vz wx wy wz ke_J erot_J epot_J etot_J fz_N\n')
            for x in r: o.write('%d %.5f '%(x[0],x[0]*1e-5)+' '.join('%.10g'%v for v in x[1:])+'\n')
        b=impacts(r)
        if not b: print('no impact',c,m); continue
        i0,i1=b[0]; pre=r[i0-2]; post=r[min(i1+1,len(r)-1)]
        E0=r[0][13]
        w=post[7:10]; wm=math.sqrt(sum(v*v for v in w))
        loss=(pre[13]-post[13])/pre[13]*100
        reb=-post[6]/pre[6]
        summ.write(f'{short[c]} {m} {n if n else 0} {corner_round(n):.6f} {r[i0][0]} {i1-i0+1} {r[i0][3]/L:.5f} {pre[6]:.6f} {post[6]:.6f} {w[0]:.4f} {w[1]:.4f} {w[2]:.4f} {wm:.4f} {post[10]:.6e} {post[11]:.6e} {post[13]:.6e} {loss:.3f} {max(x[13] for x in r)/E0:.8f} {reb:.5f}\n')
    summ.write('\n\n')
summ.close(); print('ok')

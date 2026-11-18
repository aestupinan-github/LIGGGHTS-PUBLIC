# Full-length runs (20000 steps) with per-step thermo of all relevant scalars; VTK off
import re,os,sys
B='/tmp/opencode/cmpw/dense'
TH='''variable        vx equal vcm(all,x)
variable        vy equal vcm(all,y)
variable        vz equal vcm(all,z)
variable        wx equal omega(all,x)
variable        wy equal omega(all,y)
variable        wz equal omega(all,z)
variable        fz equal fcm(all,z)
variable        xc equal xcm(all,x)
variable        yc equal xcm(all,y)
thermo          1
thermo_style    custom step v_xc v_yc v_zc v_vx v_vy v_vz v_wx v_wy v_wz c_ke c_erot v_epot v_etot v_fz
thermo_modify   format float %.12g
'''
for root,dirs,files in os.walk(B):
    if 'in' in files:
        p=os.path.join(root,'in'); s=open(p).read()
        s=re.sub(r'^thermo\s.*\n','',s,flags=re.M)
        s=re.sub(r'^thermo_style\s.*\n','',s,flags=re.M)
        s=re.sub(r'^thermo_modify\s.*\n','',s,flags=re.M)
        s=re.sub(r'^(shell\s+mkdir post\n)',TH+r'\1',s,flags=re.M)
        s=re.sub(r'^dump\s.*\n','',s,flags=re.M)
        s=re.sub(r'^run\s+\d+\s*$','run             20000',s,flags=re.M)
        s=re.sub(r'^print\s+"SQ mass.*\n','',s,flags=re.M)
        s=re.sub(r'^run\s+0\s*\n','',s,flags=re.M)
        open(p,'w').write(s)
print('ok')

import re,os
B='/tmp/opencode/cmpw/dense'
for root,dirs,files in os.walk(B):
    if 'in' not in files: continue
    p=os.path.join(root,'in'); s=open(p).read()
    s=s.replace('variable        wx equal omega(all,x)','variable        wx equal omegax[1]')
    s=s.replace('variable        wy equal omega(all,y)','variable        wy equal omegay[1]')
    s=s.replace('variable        wz equal omega(all,z)','variable        wz equal omegaz[1]')
    if 'atom_modify' not in s:
        s=re.sub(r'^(atom_style\s+surrogate.*\n)',r'\1atom_modify     map array\n',s,flags=re.M)
    s=re.sub(r'^run             20000\n\n(variable        vx)',r'\1',s,flags=re.M)
    if s.count('run             20000')>1 or not re.search(r'mkdir post\n\s*\nrun             20000|mkdir post\nrun             20000',s):
        s=re.sub(r'^run             20000\s*$','',s,flags=re.M).rstrip()+'\nrun             20000\n'
    open(p,'w').write(s)
print('ok')

"""Deterministic native-UE royal capital layout. All dimensions below are metres."""
import math,json,pathlib,random
R=pathlib.Path(__file__).parent
random.seed(7319)
items=[]; openings=[]; routes=[]
district='01_Approach'
mesh={
 'box':'/Engine/BasicShapes/Cube.Cube',
 'bevel':'/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube',
 'cylinder':'/Game/RoyalCapital/Meshes/SM_RC_Cylinder.SM_RC_Cylinder',
 'cone':'/Game/RoyalCapital/Meshes/SM_RC_Cone.SM_RC_Cone',
 'ring':'/Game/RoyalCapital/Meshes/SM_RC_Ring.SM_RC_Ring',
 'gate':'/Game/RoyalCapital/Meshes/SM_RC_Gate.SM_RC_Gate',
 'arcade':'/Game/RoyalCapital/Meshes/SM_RC_Arcade.SM_RC_Arcade',
 'window':'/Game/RoyalCapital/Meshes/SM_RC_WindowBay.SM_RC_WindowBay',
 'sphere':'/Engine/BasicShapes/Sphere.Sphere',
}
for n,stem in [('roof','SM_roof'),('column','SM_column_a'),('capital','SM_column_a_top'),('ornament','SM_ornament'),('ruin','SM_ruins_b')]:
    mesh[n]='/Game/green_Island/meshes/construction_props/'+stem+'.'+stem
for suffix in 'abcdefg': mesh['rock_'+suffix]=f'/Game/green_Island/meshes/stones/SM_stone_{suffix}.SM_stone_{suffix}'
for suffix in 'abc': mesh['debris_'+suffix]=f'/Game/green_Island/meshes/stones/SM_small_stone_{suffix}.SM_small_stone_{suffix}'
mesh['tree']='/Game/green_Island/meshes/trees/SM_tree_c.SM_tree_c'

def part(kind,pos,size,mat='Stone',yaw=0,pitch=0,roll=0,collision=True,name=''):
    items.append({'id':len(items),'name':name or kind,'district':district,'mesh':mesh.get(kind,kind),'pos':list(pos),'size':list(size),'rot':[yaw,pitch,roll],'material':mat,'collision':collision})
def box(pos,size,mat='Stone',**kw): part('box',pos,size,mat,**kw)
def cyl(x,y,z,r,h,mat='Stone',**kw): part('cylinder',(x,y,z+h/2),(r*2,r*2,h),mat,**kw)
def ring(x,y,z,r,mat='Trim',h=.12): part('ring',(x,y,z),(r*2,r*2,h),mat,collision=False)
def lerp(a,b,t): return a+(b-a)*t
def beam(a,b,width,height,mat='Trim',collision=True):
    dx,dy,dz=[b[i]-a[i] for i in range(3)]
    length=math.sqrt(dx*dx+dy*dy+dz*dz)
    yaw=math.degrees(math.atan2(dy,dx)); pitch=math.degrees(math.atan2(dz,math.hypot(dx,dy)))
    box(tuple((a[i]+b[i])/2 for i in range(3)),(length,width,height),mat,yaw=yaw,pitch=pitch,collision=collision)
def platform(rect,top,bottom=-16,holes=()):
    pieces=[rect]
    for hx0,hy0,hx1,hy1 in holes:
        nxt=[]
        for x0,y0,x1,y1 in pieces:
            ax,bx=max(x0,hx0),min(x1,hx1); ay,by=max(y0,hy0),min(y1,hy1)
            if ax>=bx or ay>=by: nxt.append((x0,y0,x1,y1)); continue
            for p in [(x0,y0,ax,y1),(bx,y0,x1,y1),(ax,y0,bx,ay),(ax,by,bx,y1)]:
                if p[2]-p[0]>.01 and p[3]-p[1]>.01: nxt.append(p)
        pieces=nxt
    for x0,y0,x1,y1 in pieces:
        box(((x0+x1)/2,(y0+y1)/2,(top+bottom)/2),(x1-x0,y1-y0,top-bottom),'DarkStone',name='TerraceStructuralSubstrate')
        box(((x0+x1)/2,(y0+y1)/2,top-.10),(x1-x0,y1-y0,.20),'Paving',name='TerraceWalkableSurface')
def stairs(a,b,width,name):
    dx,dy,dz=[b[i]-a[i] for i in range(3)]; n=math.ceil(abs(dz)/.20)
    length=math.hypot(dx,dy); yaw=math.degrees(math.atan2(dy,dx))
    px,py=-dy/length,dx/length
    for i in range(n):
        t=(i+.5)/n; top=a[2]+dz*(i+1)/n
        box((lerp(a[0],b[0],t),lerp(a[1],b[1],t),(a[2]-.25+top)/2),(length/n+.012,width,top-a[2]+.25),'Paving',yaw=yaw,name=name+'_Tread')
    for sign in [-1,1]:
        aa=(a[0]+px*sign*(width/2+.26),a[1]+py*sign*(width/2+.26),a[2]+.60)
        bb=(b[0]+px*sign*(width/2+.26),b[1]+py*sign*(width/2+.26),b[2]+.60)
        beam(aa,bb,.5,1.2,'Trim')
        for i in range(7):
            t=i/6; z=lerp(a[2],b[2],t)
            box((lerp(aa[0],bb[0],t),lerp(aa[1],bb[1],t),z+.9),(.9,.9,1.8),'Trim')
    routes.append({'id':name,'start_m':a,'end_m':b,'clear_width_m':width,'step_height_m':dz/n,'kind':'stairs'})
def wall(a,b,z,h=11,th=2.8,merlons=True):
    dx,dy=b[0]-a[0],b[1]-a[1]; length=math.hypot(dx,dy); yaw=math.degrees(math.atan2(dy,dx))
    box(((a[0]+b[0])/2,(a[1]+b[1])/2,z+h/2),(length,th,h),'Stone',yaw=yaw)
    for lev in [.3,h*.5,h-.4]:
        box(((a[0]+b[0])/2,(a[1]+b[1])/2,z+lev),(length+.15,th+.5,.55),'Trim',yaw=yaw)
    for i in range(max(1,int(length/9))+1):
        t=i/max(1,int(length/9)); x=lerp(a[0],b[0],t); y=lerp(a[1],b[1],t)
        box((x,y,z+h*.44),(1.5,th+1.3,h*.88),'DarkStone',yaw=yaw)
    if merlons:
        for i in range(max(1,int(length/3.0))):
            t=(i+.5)/max(1,int(length/3.0))
            part('bevel',(lerp(a[0],b[0],t),lerp(a[1],b[1],t),z+h+.75),(1.4,th+.3,1.5),'Stone',yaw=yaw)
def tower(x,y,z,r=4.8,h=20,spire=True):
    cyl(x,y,z,r+1.2,1.5,'DarkStone'); cyl(x,y,z+1.5,r,h-1.5)
    for lev in [1.5,h*.35,h*.7,h-.8]: cyl(x,y,z+lev,r+.25,.55,'Trim')
    cyl(x,y,z+h,r+.6,1.2,'Trim')
    for i in range(12):
        a=i*math.tau/12
        part('bevel',(x+(r+.1)*math.cos(a),y+(r+.1)*math.sin(a),z+h+1.8),(1.3,1.4,1.5),'Stone',yaw=math.degrees(a))
    if spire:
        part('cone',(x,y,z+h+1+h*.28),(r*1.85,r*1.85,h*.56),'Roof')
        cyl(x,y,z+h+1+h*.56,.11,2.2,'Metal',collision=False)
def portal(x,y,z,scale=1,yaw=0,kind='gate',name='passage'):
    native={'gate':(2,10,12),'arcade':(1,6,8.5)}[kind]
    part(kind,(x,y,z+native[2]*scale/2),tuple(a*scale for a in native),'Stone',yaw=yaw,name=name)
    openings.append({'id':name,'type':'door','geometry':mesh[kind],'boolean':'evaluated native GeometryScript SUBTRACT','position_m':[x,y,z],'yaw_deg':yaw,'clear_width_m':(6 if kind=='gate' else 4.2)*scale,'clear_height_m':(10 if kind=='gate' else 7.2)*scale,'destination':'continuous path / visible courtyard'})
def banner(x,y,z,h=4,yaw=0):
    box((x,y,z),(.08,1.65,h),'Banner',yaw=yaw,collision=False)
    box((x,y,z+h/2+.12),(.23,2.1,.23),'Metal',yaw=yaw,collision=False)
    box((x-.055*math.cos(math.radians(yaw)),y-.055*math.sin(math.radians(yaw)),z),(.03,.15,h*.60),'Trim',yaw=yaw,collision=False)
def house(x,y,z,w=13,d=12,h=12,yaw=0):
    co,si=math.cos(math.radians(yaw)),math.sin(math.radians(yaw))
    def p(lx,ly,lz): return(x+lx*co-ly*si,y+lx*si+ly*co,z+lz)
    box(p(0,0,h/2),(w-1.8,d-1.5,h),'DarkStone',yaw=yaw)
    for s in [-1,1]:
        box(p(0,s*(d/2-.35),h/2),(w,.7,h),'Stone',yaw=yaw)
    floors=max(1,round(h/6)); bays=max(2,round(d/4)); bw=d/bays
    for s in [-1,1]:
        for f in range(floors):
            bh=h/floors
            for k in range(bays):
                yy=-d/2+bw*(k+.5)
                pos=p(s*(w/2-.5),yy,bh*(f+.5))
                part('window',pos,(1,bw,bh),'Stone',yaw=yaw if s==-1 else yaw+180,name='RecessedGothicWindowBay')
                box(p(s*(w/2-1.01),yy,bh*(f+.43)),(.08,bw*.46,bh*.67),'Window',yaw=yaw,collision=False)
                box(p(s*(w/2-.6),yy,bh*(f+.43)),(.16,.14,bh*.59),'Trim',yaw=yaw,collision=False)
    for lev in [0.35,h/2,h]:
        box(p(0,0,lev),(w+.55,d+.55,.45),'Trim',yaw=yaw)
    part('roof',p(0,0,h+3.4),(w+1.1,d+1.5,6.8),'Roof',yaw=yaw)
    for sx in [-1,1]:
        for sy in [-1,1]:
            box(p(sx*(w/2-.1),sy*(d/2-.1),h/2),(.8,.8,h+1.4),'Trim',yaw=yaw)
            part('cone',p(sx*(w/2-.1),sy*(d/2-.1),h+1.6),(1.3,1.3,2.5),'Roof')
    box(p(w*.2,0,h+5),(1,1,3),'DarkStone',yaw=yaw)
    if random.random()<.6:
        bx,by,bz=p(-w/2-.10,d*.23,h*.63); banner(bx,by,bz,3,yaw)

# Three continuous structural terraces, not a floating visual shell.
district='02_Terraces'
platform((-53,-109,44,109),0,-22)
platform((34,-104,110,104),14,-22,[(34,-6,54,6),(34,-79,54,-65)])
platform((99,-99,183,99),30,-22,[(99,-6,114,6),(99,-86,114,-74)])
platform((-24,-95,29,-43),7,0)
stairs((18,0,0),(54,0,14),10,'Main_Stairs_Lower')
stairs((76,0,14),(114,0,30),10,'Main_Stairs_Upper')
stairs((18,-72,0),(54,-72,14),10,'Shortcut_Lower')
stairs((76,-80,14),(114,-80,30),10,'Shortcut_Upper')
stairs((-15,-23,0),(-15,-43,7),7,'Chapel_Stairs')

district='01_Approach'
platform((-145,-11,-123,11),0,-20)
box((-88,0,-1.0),(74,10,2),'Paving',name='EntranceBridgeDeck')
for y in [-5.6,5.6]:
    box((-88,y,.8),(74,.65,1.6),'Trim')
    for x in range(-121,-52,10):
        box((x,y,1.0),(1.1,1.1,2),'Stone')
        part('sphere',(x,y,2.25),(.7,.7,.7),'Trim')
for x in [-113,-93,-73]:
    for y in [-3.5,3.5]:
        box((x,y,-13),(3.7,3,25),'DarkStone')
    portal(x,0,-22,1.15,90,'gate','Bridge_Arch_'+str(abs(x)))
routes.append({'id':'Arrival','points_m':[[-137,0,0],[-125,0,0],[-80,0,0],[-47,0,0],[0,0,0],[18,0,0]],'clear_width_m':8})

district='03_Fortifications'
wall((-53,-109),(-53,-8),0); wall((-53,8),(-53,109),0)
portal(-53,0,0,1.3,0,'gate','Main_Gate')
tower(-53,-12,0,5.2,22); tower(-53,12,0,5.2,22)
for side in [-1,1]:
    wall((-53,side*109),(34,side*109),0)
    wall((34,side*105),(99,side*105),14)
    wall((99,side*100),(183,side*100),30,10)
    for x,y,z,h in [(-51,side*107,0,21),(31,side*107,0,23),(97,side*103,14,22),(181,side*99,30,23)]: tower(x,y,z,5,h)
wall((183,-100),(183,100),30,10)
for y in [-48,48]: tower(183,y,30,4.6,20)

district='04_MerchantQuarter'
for values in [(-34,-29,0,12,14,11,0),(-13,-24,0,11,13,13,0),(9,-31,0,12,12,10,0),(-34,30,0,12,15,12,0),(-11,31,0,14,13,15,0),(12,48,0,13,11,12,90),(-28,73,0,13,15,11,0),(-6,88,0,13,13,10,0),(20,96,0,10,11,9,90)]: house(*values)
for x in [-29,-18,-7,4]:
    for side in [-1,1]:
        y=side*14
        for sx in [-1,1]:
            box((x+sx*2.8,y,1.65),(.20,.20,3.3),'Wood')
        box((x,y,3.3),(6,3.2,.13),'Banner',pitch=8,collision=False)
        box((x,y,1.05),(5.5,1.6,.35),'Wood')
        for k in range(3): box((x-1.7+k*1.6,y,.43),(1.2,1.1,.85),'Wood')

district='05_ChapelAndGarden'
# A genuinely enterable nave, with thick side walls and a Gothic front opening.
cx,cy=12,-69
box((cx,cy,7.2),(24,18,.4),'Paving')
portal(0,cy,7,1.15,0,'gate','Chapel_Entrance')
for y in [cy-8.3,cy+8.3]:
    for x in [3,7,11,15,19,23]:
        part('window',(x,y,13.0),(1.1,4,12),'Stone',yaw=90,name='Chapel_Window')
        box((x,y+(1 if y<cy else -1)*.5,12.2),(1.7,.07,6.6),'Window',collision=False)
        box((x,y+(1 if y<cy else -1)*.6,12.2),(.15,.16,6.6),'Trim')
        box((x,y+(1 if y>cy else -1)*1.1,11.6),(1.05,2.5,9.2),'Stone')
        part('cone',(x,y+(1 if y>cy else -1)*1.5,17.8),(1.4,1.4,3.4),'Roof')
box((24,cy,13),(1,17,12),'Stone')
part('roof',(12,cy,22.5),(26,19,7),'Roof')
for y in [cy-7,cy+7]: tower(0,y,7,2.7,17)
box((19,cy,8.0),(2,4,1.6),'Trim')
for x in [-17,-12,-7]:
    for y in [-90,-85,-80]:
        box((x,y,7.45),(.85,1.9,.35),'DarkStone')
        box((x+.5,y,8.0),(.35,.75,1.6),'Stone')
for y in [-94,-44]: wall((-24,y),(29,y),7,1.0,.5,False)

district='06_CentralPlaza'
cyl(64,0,13.98,19,.22,'Paving')
for rad in [11,17.5,18.3]: ring(64,0,14.22,rad,'Trim',.10)
# Fountain offset from the straight approach lane to leave the route readable.
cyl(64,-9,14,3.4,.6,'Trim'); cyl(64,-9,14.6,2.75,.35,'Water')
cyl(64,-9,14.8,.95,3.1,'Stone'); cyl(64,-9,17.8,1.8,.35,'Trim')
part('capital',(64,-9,19.0),(1.8,1.8,2.2),None)
for values in [(58,-34,14,13,14,14,0),(83,-42,14,14,13,16,0),(63,-96,14,13,12,12,90),(66,44,14,14,15,14,0),(88,77,14,12,13,11,90),(53,81,14,13,14,14,90)]: house(*values)
for y in [-21,21]:
    for x in [57,70]:
        part('column',(x,y,16.9),(1.6,1.6,5.8),None)
        part('capital',(x,y,20.6),(1.9,1.9,2.0),None)

district='07_Cloister'
for x in [-4,2,8,14,20,26]:
    portal(x,65,0,.82,90,'arcade','Cloister_Bay_'+str(x))
    box((x,65,7.5),(6.1,5.2,.5),'Trim')
box((11,76,0.1),(38,17,.2),'Paving')
for x,y in [(-7,73),(28,73)]:
    cyl(x,y,.15,2.8,.65,'Trim')
    part('tree',(x,y,5.8),(8,8,10.2),None,collision=False)

district='08_Palace'
# Layered ceremonial facade; the main door ends at a visible leaf in a 4 m recess.
house(160,-15,30,38,64,28,0)
for y in [-46,16]: tower(138,y,30,5.3,34)
for y in [-42,12]: tower(178,y,30,4.3,35)
tower(163,-15,58,5.0,27)
portal(136,-15,30,1.60,0,'gate','Palace_Recess')
box((140,-15,37.2),(.3,8.8,14.4),'Wood')
for y in [-17.25,-12.75]:
    box((139.82,y,37.2),(.12,4.2,13.8),'Metal')
    box((139.68,y,37.2),(.12,.22,12.0),'Trim')
for y in [-37,-27,-3,7]:
    part('column',(134,y,38),(1.8,1.8,16),None)
    part('capital',(134,y,47),(2.6,2.6,3),None)
    banner(135.3,y,40,6)
for y in [-55,25]:
    for x in [144,154,164,174]:
        box((x,y,40),(1.5,4,20),'Stone')
        part('cone',(x,y,51.5),(2,2,5),'Roof')
# Upper-left patrol walk and return stair landing.
box((136,-80,30.1),(45,14,.2),'Paving')
for y in [-88,-72]:
    for x in [119,130,141,152]:
        part('column',(x,y,33),(1.2,1.2,6),None)
    box((135.5,y,36.2),(37,1.9,.65),'Trim')
routes.append({'id':'Upper_Shortcut_Return','points_m':[[114,-80,30],[124,-80,30],[124,0,30],[114,0,30]],'clear_width_m':6})

district='09_BossArena'
boss=(123,64,30.2); radius=28.5
cyl(123,64,29.9,radius,.3,'Paving')
for rad in [26.7,27.65,28.35]: ring(123,64,30.23,rad,'Purple' if rad==27.65 else 'Trim',.09)
for i in range(56):
    angle=math.tau*i/56; deg=math.degrees(angle)
    # Entrance faces south toward the royal courtyard; far exit faces east.
    if abs((deg-270+180)%360-180)<11 or abs((deg+180)%360-180)<8: continue
    x=123+radius*math.cos(angle); y=64+radius*math.sin(angle)
    box((x,y,30.85),(3.25,.50,1.30),'Trim',yaw=deg+90)
    if i%4==0:
        box((x,y,31.25),(1,1,2.1),'Stone')
        part('sphere',(x,y,32.5),(.6,.6,.6),'Trim')
portal(123,35.4,30.2,.84,90,'gate','Boss_Entrance')
portal(151.5,64,30.2,.55,0,'arcade','Boss_Exit')
routes.append({'id':'Boss_Approach','points_m':[[114,0,30],[123,10,30],[123,26,30],[123,40,30.2],[123,64,30.2]],'clear_width_m':5.0})

district='10_Terrain'
# Instanced existing rock meshes cover the stepped foundations with a cliff face.
for i in range(90):
    a=math.tau*i/90
    x=65+133*math.cos(a); y=117*math.sin(a)
    # Do not obstruct the approach bridge or the arrival landing.
    if x<-42 and abs(y)<15: continue
    sz=(random.uniform(14,26),random.uniform(12,24),random.uniform(25,48))
    part('rock_'+random.choice('abefg'),(x,y,-16),sz,None,yaw=random.randrange(360),pitch=random.uniform(-14,14),roll=random.uniform(-12,12),collision=False)
for x,y,z in [(-152,-31,-20),(-158,34,-20),(-130,-37,-19),(-128,29,-21)]:
    part('rock_g',(x,y,z),(45,43,39),None,yaw=random.randrange(360),collision=False)
for i in range(25):
    a=math.tau*i/25
    part('rock_'+random.choice('efg'),(70+620*math.cos(a),610*math.sin(a),-90),(random.uniform(140,260),random.uniform(130,220),random.uniform(100,280)),None,yaw=random.randrange(360),collision=False)
# Restrained foliage and rubble only outside reserved movement and combat areas.
for x,y,z in [(-40,-89,0),(-32,90,0),(18,88,0),(42,91,14),(76,91,14),(90,-94,14),(151,-91,30)]:
    part('tree',(x,y,z+7.5),(11,11,15),None,yaw=random.randrange(360),collision=False)
for i in range(120):
    x=random.uniform(-39,29); y=random.choice([-1,1])*random.uniform(47,101)
    if y<0 and x<30: continue # chapel platform and approach kept clean
    part('debris_'+random.choice('abc'),(x,y,.22),(random.uniform(.4,1.1),random.uniform(.4,1.3),.42),None,yaw=random.randrange(360),collision=False)

district='11_GameplayMarkers'
markers=[
 {'id':'T1','type':'TREASURE','pos':[-17,-81,7]},
 {'id':'T2','type':'TREASURE','pos':[14,32,0]},
 {'id':'T3','type':'TREASURE','pos':[26,80,0]},
 {'id':'T4','type':'TREASURE','pos':[150,-82,30]},
 {'id':'E1','type':'ENEMY','pos':[-42,0,0]},
 {'id':'E2','type':'ENEMY','pos':[-16,5,0]},
 {'id':'E3','type':'ENEMY','pos':[64,8,14]},
 {'id':'E4','type':'ENEMY','pos':[55,0,14]},
 {'id':'E5','type':'ENEMY','pos':[122,0,30]},
 {'id':'E6','type':'ENEMY','pos':[11,74,0]},
 {'id':'BOSS','type':'BOSS','pos':list(boss)}]
for m in markers:
    x,y,z=m['pos']; mat={'TREASURE':'Gold','ENEMY':'Red','BOSS':'Purple'}[m['type']]
    ring(x,y,z+.06,1.4 if m['type']!='BOSS' else 3.4,mat,.10)
    if m['type']=='ENEMY':
        cyl(x,y,z+.15,.28,1.10,mat,collision=False)
        part('sphere',(x,y,z+1.60),(.46,.46,.46),mat,collision=False)
        for sign in [-1,1]: box((x,y+sign*.20,z+.35),(.20,.18,.65),mat,collision=False)
    cyl(x,y,z+2.3,.045,1.1,mat,collision=False)
    part('box',(x,y,z+3.9),(1.15,.12,1.15),mat,yaw=140,roll=45,collision=False)

lights=[]
district='12_Lighting'
for x,y,z in [(-126,-7,0),(-126,7,0),(-60,-7,0),(-60,7,0),(-42,-9,0),(-42,9,0),(-15,-11,0),(-15,11,0),(17,-7,0),(17,7,0),(55,-7,14),(55,7,14),(74,-7,14),(74,7,14),(115,-7,30),(115,7,30),(123,31,30),(-12,-45,7),(2,-60,7),(2,-78,7),(15,67,0),(139,-40,30),(139,10,30)]:
    cyl(x,y,z,.30,.25,'Trim'); cyl(x,y,z+.25,.10,2.5,'Metal'); cyl(x,y,z+2.65,.32,.32,'Metal')
    part('sphere',(x,y,z+3.10),(.38,.38,.68),'Flame',collision=False)
    lights.append({'pos':[x,y,z+3.2],'intensity':1000,'radius':9})

data={'schema':'royal-capital.native-ue-layout.v1','units':'metres','map':'/Game/RoyalCapital/Maps/L_RoyalCapital','seed':7319,'items':items,'markers':markers,'boss':{'center_m':list(boss),'radius_m':radius,'central_clear_radius_m':24,'empty_combat_floor':True},'routes':routes,'openings':openings,'lights':lights,'spawn_m':[-137,0,1.1],'cameras':[
 {'name':'RC_Camera_Panorama','pos':[-245,-205,242],'target':[47,0,21],'fov':56},
 {'name':'RC_Camera_Entrance','pos':[-112,-1,3.2],'target':[-45,0,16],'fov':72},
 {'name':'RC_Camera_CentralPlaza','pos':[49,21,18],'target':[141,-10,46],'fov':73},
 {'name':'RC_Camera_Boss','pos':[95,33,45],'target':[130,65,34],'fov':73}]}
(R/'layout.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
(R/'openings.json').write_text(json.dumps(openings,indent=2),encoding='utf-8')
print(f'LAYOUT PASS | parts={len(items)} | markers={len(markers)} | openings={len(openings)} | boss_diameter=57m')

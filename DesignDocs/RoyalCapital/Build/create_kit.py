import unreal,math,json,pathlib
BASE='/Game/RoyalCapital'
al=unreal.EditorAssetLibrary
at=unreal.AssetToolsHelpers.get_asset_tools()
ml=unreal.MaterialEditingLibrary
GP=unreal.GeometryScript_Primitives
OPT=unreal.GeometryScriptPrimitiveOptions()
IDENT=unreal.Transform()

def material(name,color,texture=None,tile=240,emission=0,metal=0,rough=.88):
    path=BASE+'/Materials/'+name
    if al.does_asset_exist(path):
        m=al.load_asset(path)
        ml.delete_all_material_expressions(m)
    else:
        m=at.create_asset(name,BASE+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    tint=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-600,100)
    tint.set_editor_property('constant',unreal.LinearColor(*color,1))
    final=tint
    if texture:
        tx=ml.create_material_expression(m,unreal.MaterialExpressionTextureObjectParameter,-800,-250)
        tx.set_editor_property('parameter_name','Albedo')
        tx.set_editor_property('texture',al.load_asset(texture))
        pos=ml.create_material_expression(m,unreal.MaterialExpressionWorldPosition,-800,-100)
        nor=ml.create_material_expression(m,unreal.MaterialExpressionVertexNormalWS,-800,0)
        code=ml.create_material_expression(m,unreal.MaterialExpressionCustom,-300,0)
        inputs=[]
        for n in ['Tex','P','N','Tint']:
            ci=unreal.CustomInput(); ci.set_editor_property('input_name',n); inputs.append(ci)
        code.set_editor_property('inputs',inputs)
        code.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        code.set_editor_property('code',f'float3 w=pow(abs(N),4.0); w/=max(w.x+w.y+w.z,0.0001); float3 q=P/{float(tile)}; float3 c=Texture2DSample(Tex,TexSampler,q.yz).rgb*w.x+Texture2DSample(Tex,TexSampler,q.xz).rgb*w.y+Texture2DSample(Tex,TexSampler,q.xy).rgb*w.z; return lerp(dot(c,float3(0.2126,0.7152,0.0722)).xxx,c,0.7)*Tint;')
        for node,pin in [(tx,'Tex'),(pos,'P'),(nor,'N'),(tint,'Tint')]: ml.connect_material_expressions(node,'',code,pin)
        final=code
    ml.connect_material_property(final,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for value,prop,y in [(rough,unreal.MaterialProperty.MP_ROUGHNESS,300),(metal,unreal.MaterialProperty.MP_METALLIC,400)]:
        c=ml.create_material_expression(m,unreal.MaterialExpressionConstant,-300,y)
        c.set_editor_property('r',value)
        ml.connect_material_property(c,'',prop)
    if emission:
        e=ml.create_material_expression(m,unreal.MaterialExpressionConstant3Vector,-300,500)
        e.set_editor_property('constant',unreal.LinearColor(*[c*emission for c in color],1))
        ml.connect_material_property(e,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if 'Banner' in name: m.set_editor_property('two_sided',True)
    ml.recompile_material(m)
    al.save_loaded_asset(m)
    return m

tex='/Game/green_Island/textures/construction_props_textures/'
materials={
 'Stone':material('M_RC_Stone',(.70,.67,.61),tex+'wall/T_wall_basecolor',350),
 'DarkStone':material('M_RC_DarkStone',(.38,.41,.43),tex+'wall/T_wall_basecolor',350),
 'Paving':material('M_RC_Paving',(.74,.72,.66),tex+'floor_tile/T_floor_tile_a_basecolor',400),
 'Roof':material('M_RC_Roof',(.29,.34,.40),tex+'roof/T_roof_BaseColor',600),
 'Trim':material('M_RC_Trim',(.43,.41,.35),tex+'wall/T_wall_putty_baseColor',200),
 'Metal':material('M_RC_Metal',(.045,.049,.055),metal=.7,rough=.42),
 'Wood':material('M_RC_Wood',(.12,.067,.034),rough=.94),
 'Banner':material('M_RC_Banner',(.16,.012,.026)),
 'Window':material('M_RC_Window',(.11,.075,.038),emission=.7,rough=.45),
 'Gold':material('M_RC_Gold',(.9,.52,.055),emission=1.4,metal=.15),
 'Red':material('M_RC_Red',(.82,.055,.038),emission=1.8),
 'Purple':material('M_RC_Purple',(.35,.09,.85),emission=1.6),
 'Cyan':material('M_RC_Cyan',(.08,.52,.63),emission=1.0),
 'Flame':material('M_RC_Flame',(1.,.36,.065),emission=5.),
 'Water':material('M_RC_Water',(.035,.095,.10),rough=.16,metal=.35)
}

def save_mesh(name,dm,collision=True):
    opts=unreal.GeometryScriptCreateNewStaticMeshAssetOptions(enable_collision=collision,collision_mode=unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE,enable_recompute_normals=True,enable_recompute_tangents=True)
    mesh,outcome=unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm,BASE+'/Meshes/'+name,opts)
    if not mesh: raise RuntimeError(name+' creation failed '+str(outcome))
    mesh.set_material(0,materials['Stone'])
    al.save_loaded_asset(mesh)
    return mesh

def box(dm,x,y,z,loc=(0,0,0)):
    GP.append_box(dm,OPT,unreal.Transform(location=unreal.Vector(*loc)),x,y,z,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)

def subtract(target,tool):
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(target,IDENT,tool,IDENT,unreal.GeometryScriptBooleanOperation.SUBTRACT,unreal.GeometryScriptMeshBooleanOptions())

def arch_cutter(half,top,bottom,depth):
    shoulder=top-math.sqrt(3)*half
    verts=[(-half,bottom),(half,bottom),(half,shoulder)]
    for k in range(1,13):
        a=math.pi/3*k/12
        verts.append((-half+2*half*math.cos(a),shoulder+2*half*math.sin(a)))
    for k in range(1,13):
        a=2*math.pi/3+math.pi/3*k/12
        verts.append((half+2*half*math.cos(a),shoulder+2*half*math.sin(a)))
    dm=unreal.DynamicMesh()
    GP.append_simple_extrude_polygon(dm,OPT,unreal.Transform(location=unreal.Vector(-depth/2,0,0),rotation=unreal.Rotator(0,90,-90)),[unreal.Vector2D(*p) for p in verts],height=depth)
    return dm

paths={}
for name,radius,height,cone in [('SM_RC_Cylinder',50,100,False),('SM_RC_Cone',50,100,True)]:
    path=BASE+'/Meshes/'+name
    if not al.does_asset_exist(path):
        dm=unreal.DynamicMesh()
        if cone: GP.append_cone(dm,OPT,IDENT,radius,0,height,24,1,True,unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        else: GP.append_cylinder(dm,OPT,IDENT,radius,height,48,0,True,unreal.GeometryScriptPrimitiveOriginMode.CENTER)
        save_mesh(name,dm)
    paths[name]=path

# Subtract real Gothic apertures before adding masonry dressing.
for name,width,height,depth,half,top,bottom in [
 ('SM_RC_Gate',1000,1200,200,300,1000,-10),
 ('SM_RC_Arcade',600,850,100,210,720,-10),
 ('SM_RC_WindowBay',400,600,100,85,470,120)
]:
    path=BASE+'/Meshes/'+name
    if not al.does_asset_exist(path):
        dm=unreal.DynamicMesh(); box(dm,depth,width,height,(0,0,height/2))
        cutter=arch_cutter(half,top,bottom,depth+100)
        subtract(dm,cutter)
        save_mesh(name,dm)
    paths[name]=path

# Thin horizontal torus: canonical outer radius 1 m, used for inset rings.
name='SM_RC_Ring'
if not al.does_asset_exist(BASE+'/Meshes/'+name):
    dm=unreal.DynamicMesh()
    GP.append_torus(dm,OPT,IDENT,unreal.GeometryScriptRevolveOptions(),95,5,64,8,unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    save_mesh(name,dm,False)
paths[name]=BASE+'/Meshes/'+name

(pathlib.Path(__file__).parent/'kit.json').write_text(json.dumps({'meshes':paths,'materials':{k:v.get_path_name() for k,v in materials.items()}},indent=2),encoding='utf-8')
print('KIT PASS | 15 materials | 6 native meshes | Gothic openings evaluated with mesh Booleans')

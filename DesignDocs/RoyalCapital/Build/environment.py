import unreal,json,pathlib
R=pathlib.Path(__file__).parent
data=json.loads((R/'layout.json').read_text())
aas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
es=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
def actor(cls,name,loc=(0,0,0),rot=(0,0,0)):
    old=next((a for a in aas.get_all_level_actors() if a.get_actor_label()==name),None)
    a=old or aas.spawn_actor_from_class(cls,unreal.Vector(*[v*100 for v in loc]),unreal.Rotator(*rot))
    a.set_actor_label(name); a.set_folder_path('RoyalCapital/12_Lighting')
    a.set_editor_property('tags',['RC_GENERATED','RC_ENVIRONMENT'])
    return a
sun=actor(unreal.DirectionalLight,'RC_Sun',rot=(-32,-38,0))
lc=sun.get_component_by_class(unreal.DirectionalLightComponent)
lc.set_mobility(unreal.ComponentMobility.MOVABLE)
lc.set_intensity(65000)
lc.set_light_color(unreal.LinearColor(1,.89,.75,1))
lc.set_editor_property('atmosphere_sun_light',True)
lc.set_editor_property('light_source_angle',2.2)
atmos=actor(unreal.SkyAtmosphere,'RC_Atmosphere')
sky=actor(unreal.SkyLight,'RC_SkyLight')
sc=sky.get_component_by_class(unreal.SkyLightComponent)
sc.set_mobility(unreal.ComponentMobility.MOVABLE)
sc.set_intensity(1.0)
sc.set_editor_property('real_time_capture',True)
cloud=actor(unreal.VolumetricCloud,'RC_Clouds')
fog=actor(unreal.ExponentialHeightFog,'RC_RavineMist',(0,0,-26))
fc=fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_fog_density(.022)
fc.set_fog_height_falloff(.25)
fc.set_fog_inscattering_color(unreal.LinearColor(.27,.34,.42,1))
fc.set_start_distance(3500)
fc.set_fog_max_opacity(.78)
post=actor(unreal.PostProcessVolume,'RC_Exposure')
post.set_editor_property('unbound',True)
s=post.get_editor_property('settings')
for key,value in {
 'override_auto_exposure_min_brightness':True,'override_auto_exposure_max_brightness':True,
 'auto_exposure_min_brightness':13.0,'auto_exposure_max_brightness':13.0,
 'override_auto_exposure_bias':True,'auto_exposure_bias':0.0,
 'override_bloom_intensity':True,'bloom_intensity':.22,
 'override_vignette_intensity':True,'vignette_intensity':.20,
 'override_color_saturation':True,'color_saturation':unreal.Vector4(.84,.84,.84,1)
}.items(): s.set_editor_property(key,value)
post.set_editor_property('settings',s)
for i,l in enumerate(data['lights']):
    a=actor(unreal.PointLight,'RC_TorchLight_'+str(i),l['pos'])
    c=a.get_component_by_class(unreal.PointLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_intensity(l['intensity']); c.set_attenuation_radius(l['radius']*100)
    c.set_light_color(unreal.LinearColor(1,.38,.09,1)); c.set_cast_shadows(False)

nav=next((a for a in aas.get_all_level_actors() if a.get_actor_label()=='RC_NavMeshBounds'),None)
if not nav:
    dm=unreal.DynamicMesh()
    unreal.GeometryScript_Primitives.append_box(dm,unreal.GeometryScriptPrimitiveOptions(),unreal.Transform(),40000,25000,16000,origin=unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    result=unreal.GeometryScript_NewAssetUtils.create_new_volume_from_mesh(dm,world,unreal.Transform(location=unreal.Vector(2000,0,2500)),'RC_NavMeshBounds',unreal.GeometryScriptCreateNewVolumeFromMeshOptions(volume_type=unreal.NavMeshBoundsVolume))
    nav=result[0]
    assert nav is not None,'Navigation bounds creation failed'
    nav.set_actor_label('RC_NavMeshBounds'); nav.set_folder_path('RoyalCapital/00_Gameplay')
    nav.set_editor_property('tags',['RC_GENERATED','RC_NAV_BOUNDS'])
navsys=unreal.NavigationSystemV1.get_navigation_system(world)
if navsys: navsys.on_navigation_bounds_updated(nav)
unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation')
cam=next(a for a in aas.get_all_level_actors() if a.get_actor_label()=='RC_Camera_Panorama')
es.pilot_level_actor(cam)
es.editor_set_game_view(True)
es.editor_set_viewport_realtime(True)
aas.clear_actor_selection_set()
assert es.save_current_level()
print('ENVIRONMENT PASS | daylight + sky + clouds + ravine fog | torch_lights=23 | navigation bounds created')

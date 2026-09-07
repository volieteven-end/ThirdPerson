# Run on the staged project only. Builds textures/materials and rearranges preserved widget objects.
import unreal,json
from pathlib import Path
RUN=Path(r"E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v1")
ASSET="/Game/Third/UIArt/ForestUI"
LOCAL=Path(unreal.Paths.project_content_dir())/"Third/UIArt/ForestUI"
WHITE=unreal.LinearColor(1,1,1,1)
IVORY=unreal.LinearColor(0.87,0.83,0.70,1)
GOLD=unreal.LinearColor(0.58,0.40,0.15,1)
MUTED=unreal.LinearColor(0.42,0.47,0.40,1)
V=unreal.SlateVisibility
def color(r,g,b,a=1):return unreal.LinearColor(r,g,b,a)
def margin(v):return unreal.Margin(v,v,v,v)
def brush(res=None,tint=WHITE,size=(256,256),nine=False):
    return unreal.ForestUIEditorLibrary.make_brush(res,tint,size[0],size[1],nine)
def import_texture(name):
    t=unreal.AssetImportTask();t.filename=str(LOCAL/(name+".png"));t.destination_path=ASSET;t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    tex=unreal.load_asset(ASSET+"/"+name)
    tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property("lod_group",unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings",unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("srgb",True)
    tex.set_editor_property("max_texture_size",256)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex
panel_tex=import_texture("T_ForestPanel");slot_tex=import_texture("T_ForestSlot")
AT=unreal.AssetToolsHelpers.get_asset_tools()
ML=unreal.MaterialEditingLibrary
def material(name,code):
    m=unreal.load_asset(ASSET+"/"+name) if unreal.EditorAssetLibrary.does_asset_exist(ASSET+"/"+name) else AT.create_asset(name,ASSET,unreal.Material,unreal.MaterialFactoryNew())
    ML.delete_all_material_expressions(m)
    m.set_editor_property("material_domain",unreal.MaterialDomain.MD_UI)
    m.set_editor_property("blend_mode",unreal.BlendMode.BLEND_TRANSLUCENT)
    uv=ML.create_material_expression(m,unreal.MaterialExpressionTextureCoordinate,-600,0)
    p=ML.create_material_expression(m,unreal.MaterialExpressionVectorParameter,-600,160);p.set_editor_property("parameter_name","FillColor");p.set_editor_property("default_value",WHITE)
    n=ML.create_material_expression(m,unreal.MaterialExpressionCustom,-280,0);n.set_editor_property("code",code);n.set_editor_property("output_type",unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    ci1=unreal.CustomInput();ci1.set_editor_property("input_name","UV");ci2=unreal.CustomInput();ci2.set_editor_property("input_name","FillColor");n.set_editor_property("inputs",[ci1,ci2])
    assert ML.connect_material_expressions(uv,"",n,"UV");assert ML.connect_material_expressions(p,"",n,"FillColor")
    alpha=ML.create_material_expression(m,unreal.MaterialExpressionComponentMask,0,120);alpha.set_editor_property("r",False);alpha.set_editor_property("g",False);alpha.set_editor_property("b",False);alpha.set_editor_property("a",True)
    assert ML.connect_material_expressions(n,"",alpha,"")
    ML.connect_material_property(n,"",unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    ML.connect_material_property(alpha,"",unreal.MaterialProperty.MP_OPACITY)
    ML.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
    return m
fill=material("M_ForestFill","float shine=lerp(0.64,1.12,saturate(1.0-abs(UV.y-0.32)*1.8)); return float4(FillColor.rgb*shine,1.0);")
outline=material("M_ForestOutline","float d=min(min(UV.x,1.0-UV.x),min(UV.y,1.0-UV.y)); float a=1.0-smoothstep(0.012,0.025,d); return float4(FillColor.rgb,a);")
def instance(name,tint):
    mi=unreal.load_asset(ASSET+"/"+name) if unreal.EditorAssetLibrary.does_asset_exist(ASSET+"/"+name) else AT.create_asset(name,ASSET,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    ML.set_material_instance_parent(mi,fill);ML.set_material_instance_vector_parameter_value(mi,"FillColor",tint);ML.update_material_instance(mi);unreal.EditorAssetLibrary.save_loaded_asset(mi);return mi
hp=instance("MI_ForestHealth",color(.62,.034,.025));sp=instance("MI_ForestStamina",color(.09,.40,.12));xp=instance("MI_ForestExperience",color(.035,.38,.48))
def load_tree(name,root):
    bp=unreal.load_asset("/Game/Third/Widget/"+name);tree=unreal.load_object(None,bp.get_path_name()+":WidgetTree");root=unreal.load_object(None,tree.get_path_name()+"."+root);return bp,tree,root
bp,tree,root=load_tree("WBP_Inventory","CanvasPanel_290")
widgets={}
def collect(w):
    widgets[w.get_name()]=w
    if isinstance(w,unreal.PanelWidget):
        for c in w.get_all_children():collect(c)
collect(root)
button_children={w.get_name():w.get_child_at(0) for w in widgets.values() if isinstance(w,unreal.Button) and w.get_children_count()}
def new(cls,name):return unreal.new_object(cls,outer=tree,name=name)
def txt(w,text=None,size=20,tint=IVORY):
    if text is not None:w.set_text(text)
    font=w.get_editor_property("font");font.set_editor_property("size",size);font.set_editor_property("typeface_font_name","Regular");w.set_font(font)
    w.set_color_and_opacity(unreal.SlateColor(specified_color=tint));w.set_auto_wrap_text(True)
    w.set_shadow_offset(unreal.Vector2D(1,1));w.set_shadow_color_and_opacity(color(0,0,0,.6));w.set_visibility(V.HIT_TEST_INVISIBLE)
    return w
def text(name,value,size=20,tint=IVORY):return txt(new(unreal.TextBlock,name),value,size,tint)
def canvas_add(parent,w,x,y,width,height,anchor=(0,0),align=(0,0),z=0,stretch=False):
    w.remove_from_parent();sl=parent.add_child_to_canvas(w)
    sl.set_anchors(unreal.Anchors(unreal.Vector2D(*anchor),unreal.Vector2D(*(1,1) if stretch else anchor)))
    sl.set_alignment(unreal.Vector2D(*align));sl.set_position(unreal.Vector2D(x,y));sl.set_size(unreal.Vector2D(width,height));sl.set_z_order(z)
    if stretch:sl.set_offsets(unreal.Margin(0,0,0,0))
    return sl
def border(name,res=panel_tex,tint=WHITE):
    b=new(unreal.Border,name);b.set_brush(brush(res,tint,(128,128),nine=True));b.set_padding(margin(0));b.set_visibility(V.SELF_HIT_TEST_INVISIBLE);return b
def box(w,name,width,height):
    b=new(unreal.SizeBox,name);b.set_width_override(width);b.set_height_override(height);w.remove_from_parent();b.add_child(w);return b
def button(w,label):
    st=w.get_editor_property("widget_style")
    st.set_editor_property("normal",brush(slot_tex,WHITE,(128,128),True))
    st.set_editor_property("hovered",brush(slot_tex,color(1.35,1.30,1.12,1),(128,128),True))
    st.set_editor_property("pressed",brush(slot_tex,color(.7,.7,.65,1),(128,128),True))
    st.set_editor_property("disabled",brush(slot_tex,color(.45,.45,.45,1),(128,128),True))
    st.set_editor_property("normal_padding",margin(8));st.set_editor_property("pressed_padding",margin(8));w.set_style(st);w.set_background_color(WHITE)
    if label:
        c=w.get_child_at(0)
        if c is None:
            c=button_children.get(w.get_name()) or text("Forest"+w.get_name()+"Label",label,20)
            c.remove_from_parent();w.add_child(c)
        if isinstance(c,unreal.TextBlock):txt(c,label,20)
    return w
def progress(w,mat):
    st=w.get_editor_property("widget_style");st.set_editor_property("background_image",brush(None,color(.012,.018,.014,1)))
    st.set_editor_property("fill_image",brush(mat,WHITE,(256,24)));w.set_editor_property("widget_style",st);w.set_fill_color_and_opacity(WHITE);w.set_editor_property("border_padding",unreal.Vector2D(0,0))
    w.set_editor_property("bar_fill_type",unreal.ProgressBarFillType.LEFT_TO_RIGHT)
    w.set_editor_property("bar_fill_style",unreal.ProgressBarFillStyle.MASK)
    w.set_visibility(V.HIT_TEST_INVISIBLE)
def line(parent,name,x,y,width,height=1):canvas_add(parent,border(name,None,GOLD),x,y,width,height)
# Preserve original named widget objects and all existing event/function graphs.
for w in list(widgets.values()):
    if w!=root:w.remove_from_parent()
root.clear_children()
hud=widgets["HUDLayer"];hud.clear_children();canvas_add(root,hud,0,0,0,0,stretch=True)
status=border("ForestStatusPanel",slot_tex,color(.32,.38,.34));canvas_add(hud,status,32,32,480,162)
sc=new(unreal.CanvasPanel,"ForestStatusCanvas");status.add_child(sc)
for name,y in [("HP_image",20),("Stamina_image",59)]:
    w=widgets[name];w.set_visibility(V.HIT_TEST_INVISIBLE);canvas_add(sc,w,20,y,26,26)
progress(widgets["HPbar"],hp);progress(widgets["StaminaBar"],sp);progress(widgets["ExperienceProgress"],xp)
canvas_add(sc,widgets["HPbar"],58,23,264,22);canvas_add(sc,widgets["StaminaBar"],58,64,264,16);canvas_add(sc,widgets["ExperienceProgress"],58,105,264,8)
for name,value,y in [("HealthTextBlock","HP: 100 / 100",18),("StaminaTextBlock","SP: 100 / 100",55)]:
    w=txt(widgets[name],value,16);w.set_editor_property("justification",unreal.TextJustify.RIGHT);canvas_add(sc,w,332,y,132,30)
canvas_add(sc,text("ForestXPLabel","XP",16,GOLD),20,99,34,22)
canvas_add(sc,txt(widgets["LevelTextBlock"],"Lv.1  XP 0 / 100",16,MUTED),58,124,380,24)
obj=border("ForestObjectivePanel",slot_tex,color(.32,.38,.34));canvas_add(hud,obj,-32,32,340,152,(1,0),(1,0))
oc=new(unreal.CanvasPanel,"ForestObjectiveCanvas");obj.add_child(oc)
w=txt(widgets["ObjectTextBlock"],"清理区域\n击败敌人 0 / 20",20);w.set_editor_property("justification",unreal.TextJustify.RIGHT);canvas_add(oc,w,20,20,300,72)
line(oc,"ForestObjectiveDivider",20,101,300)
w=txt(widgets["KillTextBlock"],"Kills: 0",18,MUTED);w.set_editor_property("justification",unreal.TextJustify.RIGHT);canvas_add(oc,w,20,113,300,25)
w=txt(widgets["InteractionTextBlock"],"",22);w.set_editor_property("justification",unreal.TextJustify.CENTER);canvas_add(root,w,0,-40,520,40,(.5,1),(.5,1),3);w.set_visibility(V.COLLAPSED)
# Existing InventoryPanel becomes a full-screen scrim containing a centered framed panel.
inv=widgets["InventoryPanel"];inv.clear_children();inv.set_brush(brush(None,color(0,0,0,.5)));inv.set_brush_color(WHITE);inv.set_padding(margin(0));inv.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_FILL);inv.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL);canvas_add(root,inv,0,0,0,0,z=20,stretch=True)
ic=new(unreal.CanvasPanel,"ForestInventoryCanvas");inv.add_child(ic)
frame=border("ForestInventoryFrame",panel_tex,color(.38,.43,.4));canvas_add(ic,frame,0,0,1088,744,(.5,.5),(.5,.5))
fc=new(unreal.CanvasPanel,"ForestInventoryContent");frame.add_child(fc)
canvas_add(fc,txt(widgets["Inventory"],"背包",28),48,36,400,48)
canvas_add(fc,text("ForestCapacity","20 格",18,MUTED),170,44,160,28)
canvas_add(fc,button(widgets["CloseButton"],"关闭"),-36,34,108,44,(1,0),(1,0))
line(fc,"ForestHeaderDivider",36,88,1016)
grid=widgets["InventoryGrid"];grid.clear_children();grid.set_slot_padding(margin(8));grid.set_min_desired_slot_width(112);grid.set_min_desired_slot_height(112);canvas_add(fc,grid,32,112,560,448)
line(fc,"ForestDetailsDivider",624,112,1,448)
canvas_add(fc,text("ForestEmptySelection","选择物品以查看详情",20,MUTED),666,284,340,64)
selected=widgets["SelectedPanel"];selected.clear_children();canvas_add(fc,selected,666,118,348,424)
im=new(unreal.Image,"ForestSelectedItemImage");im.set_visibility(V.COLLAPSED)
sl=selected.add_child_to_vertical_box(box(im,"ForestSelectedImageSize",148,148));sl.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);sl.set_padding(unreal.Margin(0,0,0,16))
selected.add_child_to_vertical_box(box(txt(widgets["SelectedItemNameText"],"",26),"ForestSelectedNameSize",348,60))
selected.add_child_to_vertical_box(box(txt(widgets["SelectedItemCountText"],"",20,MUTED),"ForestSelectedCountSize",348,50))
actions=widgets["UseandDropBox"];actions.clear_children()
for name,label in [("UseButton","使用"),("DropBUtton","丢弃")]:
    b=button(widgets[name],label);b.set_visibility(V.VISIBLE);sl=actions.add_child_to_horizontal_box(box(b,"Forest"+name+"Size",154,52));sl.set_padding(unreal.Margin(0,0,16,0))
selected.add_child_to_vertical_box(actions);selected.set_visibility(V.HIDDEN)
canvas_add(fc,txt(widgets["InventoryHintText"],"",18,GOLD),40,594,970,56)
w=widgets["InventoryTextBlock"];canvas_add(fc,w,0,0,1,1);w.set_visibility(V.COLLAPSED)
line(fc,"ForestFooterDivider",36,666,1016)
canvas_add(fc,text("ForestInventoryHelp","选择物品后，可使用或丢弃",18,MUTED),48,686,900,30)
inv.set_visibility(V.COLLAPSED)
# Level-up page keeps all existing choice event bindings and text widgets.
lev=widgets["LevelUpPanel"];lev.clear_children();canvas_add(root,lev,0,0,0,0,z=30,stretch=True)
shade=border("ForestUpgradeScrim",None,color(0,0,0,.55));canvas_add(lev,shade,0,0,0,0,stretch=True)
lf=border("ForestUpgradeFrame",panel_tex,color(.38,.43,.4));canvas_add(lev,lf,0,0,1088,632,(.5,.5),(.5,.5));lc=new(unreal.CanvasPanel,"ForestUpgradeContent");lf.add_child(lc)
canvas_add(lc,txt(widgets["ChooseanUpgrade"],"选择一项强化",32),40,28,960,48)
line(lc,"ForestUpgradeDivider",36,94,1016)
for i,key in enumerate(["A","B","C"]):
    b=button(widgets["ChooseButton"+key],None);b.clear_children()
    cv=new(unreal.CanvasPanel,"ForestUpgradeCard"+key);b.add_child(cv)
    canvas_add(cv,text("ForestUpgradeRoman"+key,["Ⅰ","Ⅱ","Ⅲ"][i],44,GOLD),24,24,240,64)
    canvas_add(cv,txt(widgets["ChoiceATitleText_"+str(i)],["生命强化","精力强化","攻击强化"][i],24),24,111,240,64)
    canvas_add(cv,txt(widgets["ChoiceADescriptionText_"+str(i)],"选择后获得本次强化效果",20,MUTED),24,193,240,130)
    canvas_add(lc,b,40+i*338,120,312,410)
canvas_add(lc,text("ForestUpgradeHelp","选择后继续战斗",18,MUTED),40,567,950,30);lev.set_visibility(V.COLLAPSED)
# Result and restart controls preserve their independent visibility events.
w=txt(widgets["ResultTextBlock"],"",46,GOLD);w.set_editor_property("justification",unreal.TextJustify.CENTER);canvas_add(root,w,0,-100,900,120,(.5,.5),(.5,.5),40);w.set_visibility(V.HIDDEN)
for name,label,x in [("RestartButton","重新开始",-142),("NewGameButton","新游戏",142)]:
    b=button(widgets[name],label);canvas_add(root,b,x,55,256,60,(.5,.5),(.5,.5),40);b.set_visibility(V.HIDDEN)
assert unreal.ForestUIEditorLibrary.connect_graph_pins(bp,"K2Node_Event_16","ChoiceCTitle","K2Node_CallFunction_45","InText")
assert unreal.ForestUIEditorLibrary.connect_graph_pins(bp,"K2Node_Event_16","ChoiceCDescription","K2Node_CallFunction_46","InText")
cdo=unreal.get_default_object(bp.generated_class());cdo.set_editor_property("inventory_slot_widget_class",unreal.load_class(None,"/Game/Third/Widget/WBP_InventorySlot.WBP_InventorySlot_C"));cdo.set_editor_property("inventory_grid_columns",5);bp.modify();unreal.ForestUIEditorLibrary.finalize_widget_blueprint(bp);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
# Slot widget: same Button, Border, Image and Text objects, square tile with overlaid count.
sb,tree,sroot=load_tree("WBP_InventorySlot","VerticalBox_174")
sw={}
def cslot(w):
    sw[w.get_name()]=w
    if isinstance(w,unreal.PanelWidget):
        for c in w.get_all_children():cslot(c)
cslot(sroot)
for w in list(sw.values()):
    if w!=sroot:w.remove_from_parent()
sroot.clear_children();size=sw["SizeBox_0"];size.clear_children();size.set_width_override(96);size.set_height_override(96)
sroot.add_child_to_vertical_box(size)
b=button(sw["SlotButton"],None);b.clear_children();size.add_child(b)
st=b.get_editor_property("widget_style");st.set_editor_property("normal_padding",margin(0));st.set_editor_property("pressed_padding",margin(0));b.set_style(st)
o=new(unreal.Overlay,"ForestSlotOverlay");b.add_child(o)
frame=sw["SlotBorder"];frame.clear_children();frame.set_brush(brush(outline,WHITE,(96,96)));frame.set_brush_color(color(.22,.18,.10,1));frame.set_padding(margin(6))
cc=new(unreal.CanvasPanel,"ForestSlotContents");frame.add_child(cc)
o.add_child_to_overlay(frame)
im=sw["ItemIcon"];im.set_visibility(V.COLLAPSED);canvas_add(cc,im,0,0,68,68,(.5,.5),(.5,.5))
n=txt(sw["ItemNameText"],"",15,MUTED);n.set_editor_property("justification",unreal.TextJustify.CENTER);canvas_add(cc,n,0,0,78,62,(.5,.5),(.5,.5));n.set_visibility(V.COLLAPSED)
cnt=txt(sw["ItemCountText"],"",17);cnt.set_editor_property("justification",unreal.TextJustify.RIGHT);canvas_add(cc,cnt,-2,-2,70,22,(1,1),(1,1));cnt.set_visibility(V.COLLAPSED)
sb.modify();unreal.ForestUIEditorLibrary.finalize_widget_blueprint(sb);unreal.BlueprintEditorLibrary.compile_blueprint(sb);unreal.EditorAssetLibrary.save_loaded_asset(sb,False)
# Refresh parent after slot blueprint compilation.
unreal.ForestUIEditorLibrary.finalize_widget_blueprint(bp);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
for b in [bp,sb]:
    task=unreal.AssetExportTask();task.object=b;task.filename=str(RUN/(b.get_name()+"_modified.t3d"));task.automated=True;task.prompt=False;task.replace_identical=True;task.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(task)
print("FOREST_UI_ASSETS_AND_WIDGETS_BUILT")


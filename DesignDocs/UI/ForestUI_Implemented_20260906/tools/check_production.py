import unreal,json
from pathlib import Path
out=Path(r'E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v1')
result={}
for name in ['WBP_InventorySlot','WBP_Inventory']:
    bp=unreal.load_asset('/Game/Third/Widget/'+name)
    assert bp
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    result[name]={'status':str(bp.get_editor_property('status')),'class':bp.generated_class().get_path_name()}
bp=unreal.load_asset('/Game/Third/Widget/WBP_Inventory')
tree=unreal.load_object(None,bp.get_path_name()+':WidgetTree')
for name in ['ForestInventoryFrame','ForestStatusPanel','ForestUpgradeFrame','ForestSelectedItemImage','InventoryGrid','HPbar','StaminaBar','ExperienceProgress']:
    assert unreal.load_object(None,tree.get_path_name()+'.'+name),name
for name in ['M_ForestFill','M_ForestOutline']:
    m=unreal.load_asset('/Game/Third/UIArt/ForestUI/'+name)
    assert m.get_editor_property('material_domain')==unreal.MaterialDomain.MD_UI
for name in ['T_ForestPanel','T_ForestSlot']:
    t=unreal.load_asset('/Game/Third/UIArt/ForestUI/'+name)
    result[name]={'source':t.get_editor_property('asset_import_data').get_first_filename(),'max_texture_size':t.get_editor_property('max_texture_size')}
result['reopened']=True
(out/'production_asset_check.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('PRODUCTION assets=PASS widgets=PASS materials=PASS')

import unreal,json,re
from pathlib import Path
out=Path(r"E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v1")
cmd=unreal.SystemLibrary.get_command_line()
phase=re.search(r"-ForestPhase=(\w+)",cmd).group(1)
expect=phase=="modified"
for asset in ["WBP_InventorySlot","WBP_Inventory"]:
    unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset("/Game/Third/Widget/"+asset))
w=unreal.ForestUIEditorLibrary.create_test_widget()
assert w
result=json.loads(unreal.ForestUIEditorLibrary.test_inventory(w,expect))
if not result["pass"]:
    unreal.ForestUIEditorLibrary.cleanup_test_widget(w)
    raise AssertionError(result)
result["phase"]=phase
if "-ForestRender" in cmd and "-ForestDeferred" not in cmd:
    w.set_inventory_panel_open(False)
    assert unreal.ForestUIEditorLibrary.render_widget(w,str(out/(phase+"_hud.png")),1920,1080)
    w.set_inventory_panel_open(True)
    assert unreal.ForestUIEditorLibrary.render_widget(w,str(out/(phase+"_inventory.png")),1920,1080)
if expect:
    bp=unreal.load_asset("/Game/Third/Widget/WBP_Inventory")
    tree=unreal.load_object(None,bp.get_path_name()+":WidgetTree")
    panel=unreal.load_object(None,tree.get_path_name()+".ForestInventoryFrame")
    assert panel
    assert unreal.EditorAssetLibrary.does_asset_exist("/Game/Third/UIArt/ForestUI/MI_ForestHealth")
    assert unreal.EditorAssetLibrary.does_asset_exist("/Game/Third/UIArt/ForestUI/MI_ForestStamina")
    assert unreal.EditorAssetLibrary.does_asset_exist("/Game/Third/UIArt/ForestUI/MI_ForestExperience")
    result["layout"]=True;result["materials"]=True
(out/(phase+"_test.json")).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
print(phase.upper()+" core=PASS hp_percent="+("PASS" if result["hp_percent_matches_50"] else "FAIL")+" grid=20"+(" layout=PASS materials=PASS" if expect else ""))


if "-ForestDeferred" not in cmd: unreal.ForestUIEditorLibrary.cleanup_test_widget(w)

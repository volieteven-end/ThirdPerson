import json
from pathlib import Path
import unreal

run = Path('E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v2_hud_lock')
original = json.loads((run / 'baseline_test.json').read_text(encoding='utf-8'))
result = {}
bp = unreal.load_asset('/Game/Third/Widget/WBP_Inventory')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert str(bp.get_editor_property('status')).find('BS_UP_TO_DATE') >= 0
tree = unreal.load_object(None, bp.get_path_name() + ':WidgetTree')
for name, alpha in [('ForestStatusPanel', .35), ('ForestObjectivePanel', .32)]:
    panel = unreal.load_object(None, tree.get_path_name() + '.' + name)
    assert abs(panel.get_editor_property('brush_color').a - alpha) < .0001
    assert panel.get_render_opacity() == 1
    size = panel.get_editor_property('slot').get_size()
    result[name] = {'alpha': round(panel.get_editor_property('brush_color').a, 2), 'size': [size.x, size.y]}
assert result['ForestObjectivePanel']['size'] == [280., 112.]
result['WBP_Inventory'] = str(bp.get_editor_property('status'))
for name, before in original['enemy_defaults'].items():
    bp = unreal.load_asset('/Game/Third/Character/' + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert 'BS_UP_TO_DATE' in str(bp.get_editor_property('status'))
    cdo = unreal.get_default_object(bp.generated_class())
    assert str(cdo.get_editor_property('lock_on_indicator_socket_name')) == 'spine_03'
    assert cdo.get_editor_property('lock_on_indicator_size') == 24.
    camera = cdo.get_editor_property('lock_on_camera_reference_offset')
    assert [camera.x, camera.y, camera.z] == before['lock_on_indicator_offset']
    result[name] = {'status': str(bp.get_editor_property('status')), 'socket': 'spine_03', 'size': 24,
                    'camera_reference_offset': [camera.x, camera.y, camera.z]}
result['pass'] = True
(run / 'production_check.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print('PRODUCTION widgets=PASS enemy_blueprints=PASS camera_settings=PASS')

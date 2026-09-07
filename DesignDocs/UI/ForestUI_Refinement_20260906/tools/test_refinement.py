import json
import re
from pathlib import Path
import unreal

out = Path('E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v2_hud_lock')
command = unreal.SystemLibrary.get_command_line()
phase = re.search(r'-ForestPhase=(\w+)', command).group(1)
expected = phase == 'modified'
bp = unreal.load_asset('/Game/Third/Widget/WBP_Inventory')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
tree = unreal.load_object(None, bp.get_path_name() + ':WidgetTree')

def widget(name):
    result = unreal.load_object(None, tree.get_path_name() + '.' + name)
    assert result, name
    return result

def panel(name):
    value = widget(name)
    tint = value.get_editor_property('background').get_editor_property('tint_color').get_editor_property('specified_color')
    background = value.get_editor_property('brush_color')
    size = value.get_editor_property('slot').get_size()
    return {'background_alpha': round(tint.a * background.a, 5),
            'render_opacity': value.get_render_opacity(), 'size': [size.x, size.y]}

result = {'phase': phase,
          'status': panel('ForestStatusPanel'),
          'objective': panel('ForestObjectivePanel'),
          'objective_font': widget('ObjectTextBlock').get_editor_property('font').get_editor_property('size'),
          'health_text_opacity': widget('HealthTextBlock').get_render_opacity()}
defaults = {}
for name in ['BP_EnemyCharacter', 'BP_EnemyRangedCharacter']:
    enemy_bp = unreal.load_asset('/Game/Third/Character/' + name)
    unreal.BlueprintEditorLibrary.compile_blueprint(enemy_bp)
    cdo = unreal.get_default_object(enemy_bp.generated_class())
    values = {}
    for field in ['lock_on_indicator_socket_name', 'lock_on_indicator_offset', 'lock_on_indicator_size',
                  'lock_on_indicator_pulse_amount', 'lock_on_indicator_pulse_speed', 'lock_on_camera_aim_below_indicator']:
        value = cdo.get_editor_property(field)
        values[field] = [value.x, value.y, value.z] if isinstance(value, unreal.Vector) else value if isinstance(value, (int, float)) else str(value)
    defaults[name] = values
result['enemy_defaults'] = defaults

w = unreal.ForestUIEditorLibrary.create_test_widget()
assert w
result['inventory'] = json.loads(unreal.ForestUIEditorLibrary.test_inventory(w, True))
result['lock'] = json.loads(unreal.ForestUIEditorLibrary.test_lock_marker(w, expected))
result['hud_refined'] = (result['status']['background_alpha'] <= 0.36 and
                         result['objective']['background_alpha'] <= 0.33 and
                         result['objective']['size'] == [280.0, 112.0] and
                         result['objective_font'] == 16 and result['health_text_opacity'] == 1)
result['pass'] = result['inventory']['pass'] and result['lock']['pass'] and (not expected or result['hud_refined'])
if expected:
    baseline = json.loads((out / 'baseline_test.json').read_text(encoding='utf-8'))
    def xyz(text):
        return [float(v) for v in re.findall(r'[XYZ]=([-\d.]+)', text)]
    result['camera_aim_preserved'] = all(all(abs(x - y) < .01 for x, y in zip(xyz(a['aim_world']), xyz(b['aim_world'])))
                                        for a, b in zip(result['lock']['cases'], baseline['lock']['cases']))
    result['pass'] = result['pass'] and result['camera_aim_preserved']

(out / (phase + '_test.json')).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding='utf-8')
task = unreal.AssetExportTask()
task.object = bp
task.filename = str(out / (phase + '_WBP_Inventory.t3d'))
task.automated = True
task.prompt = False
task.replace_identical = True
task.exporter = unreal.ObjectExporterT3D()
assert unreal.Exporter.run_asset_export_task(task)
assert result['pass'], result
print(phase.upper() + ' core=PASS hud=' + ('LIGHT' if result['hud_refined'] else 'DARK') +
      ' marker=' + ('BODY_ORB' if result['lock']['refined'] else 'LEGACY_RING'))
if '-ForestDeferred' not in command:
    unreal.ForestUIEditorLibrary.cleanup_test_widget(w)

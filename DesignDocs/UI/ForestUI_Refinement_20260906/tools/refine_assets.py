import json
from pathlib import Path
import unreal

run = Path('E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v2_hud_lock')
baseline = json.loads((run / 'baseline_test.json').read_text(encoding='utf-8'))
bp = unreal.load_asset('/Game/Third/Widget/WBP_Inventory')
tree = unreal.load_object(None, bp.get_path_name() + ':WidgetTree')

def w(name):
    value = unreal.load_object(None, tree.get_path_name() + '.' + name)
    assert value, name
    value.modify()
    return value

def rect(name, x, y, width, height):
    slot = w(name).get_editor_property('slot')
    slot.set_position(unreal.Vector2D(x, y))
    slot.set_size(unreal.Vector2D(width, height))

for name, alpha in [('ForestStatusPanel', .35), ('ForestObjectivePanel', .32)]:
    panel = w(name)
    color = panel.get_editor_property('brush_color')
    color.a = alpha
    panel.set_brush_color(color)
    # Only the backdrop fades. Child text, icons and progress fills remain fully opaque.
    panel.set_render_opacity(1)

w('ForestObjectivePanel').get_editor_property('slot').set_size(unreal.Vector2D(280, 112))
rect('ObjectTextBlock', 16, 12, 248, 58)
rect('ForestObjectiveDivider', 16, 75, 248, 1)
rect('KillTextBlock', 16, 83, 248, 22)
for name, size in [('ObjectTextBlock', 16), ('KillTextBlock', 14)]:
    text = w(name)
    font = text.get_editor_property('font')
    font.set_editor_property('size', size)
    text.set_font(font)
w('ForestObjectiveDivider').set_brush_color(unreal.LinearColor(1, 1, 1, .45))
bp.modify()
unreal.ForestUIEditorLibrary.finalize_widget_blueprint(bp)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)

# Preserve each enemy's existing camera tuning before moving its visual marker.
# In particular, the melee enemy has a saved -70 Z reference offset and 30-unit aim drop.
for name, old in baseline['enemy_defaults'].items():
    enemy_bp = unreal.load_asset('/Game/Third/Character/' + name)
    cdo = unreal.get_default_object(enemy_bp.generated_class())
    cdo.set_editor_property('lock_on_camera_reference_socket_name', unreal.Name(old['lock_on_indicator_socket_name']))
    cdo.set_editor_property('lock_on_camera_reference_offset', unreal.Vector(*old['lock_on_indicator_offset']))
    cdo.set_editor_property('lock_on_camera_aim_below_indicator', old['lock_on_camera_aim_below_indicator'])
    cdo.set_editor_property('lock_on_indicator_socket_name', unreal.Name('spine_03'))
    cdo.set_editor_property('lock_on_indicator_offset', unreal.Vector(0, 0, 0))
    cdo.set_editor_property('lock_on_indicator_size', 24.)
    cdo.set_editor_property('lock_on_indicator_pulse_amount', .8)
    cdo.set_editor_property('lock_on_indicator_pulse_speed', 3.)
    enemy_bp.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(enemy_bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(enemy_bp, False)
print('REFINE_ASSETS backgrounds=0.35/0.32 objective=280x112 enemy_camera_settings=PRESERVED')

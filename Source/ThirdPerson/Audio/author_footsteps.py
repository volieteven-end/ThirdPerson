# 编辑器写入工具：为目标动画配置脚步通知，执行前核对资源范围并保存编辑器中的修改。
"""Run through UE's PythonScript commandlet after building the native audio classes.

Only adds/replaces the TPC_Footsteps track on the two live locomotion blend spaces'
sequence dependencies. Existing animation data and other notify tracks stay intact.
"""
import json
import os
import unreal

TRACK = 'TPC_Footsteps'
LIB = unreal.AnimationLibrary
paths = set()
for name in ('BS_SwordLocomotion', 'BS_Crouch_Locomotion'):
    blend = unreal.load_asset('/Game/Third/Input/' + name)
    assert blend.get_editor_property('notify_trigger_mode') == unreal.NotifyTriggerMode.HIGHEST_WEIGHTED_ANIMATION
    for sample in blend.get_editor_property('sample_data'):
        anim = sample.get_editor_property('animation')
        if anim and '/Actions/Sword/Sequences/' in anim.get_path_name():
            paths.add(anim.get_path_name())
assert len(paths) == 25, 'Review changed locomotion dependencies before authoring'

report = []
for path in sorted(paths):
    anim = unreal.load_asset(path)
    duration = anim.get_play_length()
    count = round(duration * 90)
    heights = {'ball_l': [], 'ball_r': []}
    for index in range(count):
        pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(
            anim, duration * index / count, unreal.AnimPoseEvaluationOptions())
        for foot in heights:
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, foot, unreal.AnimPoseSpaces.WORLD)
            heights[foot].append(transform.translation.z)
    # First near-ground descending sample after each foot's swing peak. Taking the
    # absolute minimum alone would place some walking notifies late in stance.
    events = []
    for foot, values in heights.items():
        low, high = min(values), max(values)
        threshold = low + max(.45, min(2., (high - low) * .12))
        peak = values.index(high)
        contact = next((peak + offset) % count for offset in range(1, count)
                       if values[(peak + offset) % count] <= threshold)
        events.append((duration * contact / count, foot, values[contact], threshold))
    LIB.remove_animation_notify_track(anim, TRACK)
    LIB.add_animation_notify_track(anim, TRACK, unreal.LinearColor(.2, .7, 1., 1.))
    for time, foot, height, threshold in sorted(events):
        notify = LIB.add_animation_notify_event(anim, TRACK, time, unreal.TPCFootstepNotify)
        notify.set_editor_property('foot_bone', foot)
        report.append({'animation': path, 'time': time, 'foot': foot,
                       'height_cm': height, 'contact_threshold_cm': threshold})
    assert unreal.EditorAssetLibrary.save_loaded_asset(anim, only_if_is_dirty=False)
    unreal.log('FOOTSTEP_AUTHORED ' + path + ' ' + str(events))

output = os.path.join(unreal.Paths.project_saved_dir(), 'CharacterAudio')
os.makedirs(output, exist_ok=True)
with open(os.path.join(output, 'authored_footsteps.json'), 'w') as stream:
    json.dump(report, stream, indent=2)
unreal.log('FOOTSTEPS_COMPLETE animations=25 contacts=50')

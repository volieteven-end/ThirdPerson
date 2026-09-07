import unreal
from pathlib import Path

p = Path('E:/UNREAL/ue projects/ThirdPerson/Saved/ForestUIBuild/20260906_v2_hud_lock/test_refinement.py')
exec(compile(p.read_text(encoding='utf-8'), str(p), 'exec'))
frames = 0
handle = None

def render_tick(dt):
    global frames, handle
    frames += 1
    try:
        if frames == 20:
            assert unreal.ForestUIEditorLibrary.render_widget(w, str(out / (phase + '_hud.png')), 1920, 1080)
        elif frames == 40:
            assert unreal.ForestUIEditorLibrary.render_widget(w, str(out / (phase + '_hud.png')), 1920, 1080)
            assert unreal.ForestUIEditorLibrary.render_widget(w, str(out / (phase + '_hud_720p.png')), 1280, 720)
            assert unreal.ForestUIEditorLibrary.render_lock_marker(w, str(out / (phase + '_orb_dark.png')), False)
            assert unreal.ForestUIEditorLibrary.render_lock_marker(w, str(out / (phase + '_orb_light.png')), True)
            print('REFINEMENT_RENDER_COMPLETE')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.ForestUIEditorLibrary.cleanup_test_widget(w)
            unreal.SystemLibrary.quit_editor()
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
        raise

handle = unreal.register_slate_post_tick_callback(render_tick)

"""Read-only geometry/portal audit for the additive arena expansion."""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for name in ["SM_wall_arch", "SM_column_c", "SM_floor_tile"]:
    mesh = unreal.load_asset("/Game/green_Island/meshes/construction_props/" + name)
    unreal.log("ARENA_MESH " + name + " bounds=" + str(mesh.get_bounds()))
for path in ["/Game/Third/Tutorial/Maps/L_ForestTutorial", "/Game/Third/Bosses/Countess/Maps/L_CountessBossTest"]:
    levels.load_level(path)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, (unreal.PlayerStart, unreal.ArenaPortal, unreal.TutorialDirector)):
            unreal.log("ARENA_ACTOR " + path + " " + actor.get_actor_label() + " pos=" + str(actor.get_actor_location()) + " rot=" + str(actor.get_actor_rotation()))
            if isinstance(actor, unreal.TutorialDirector):
                unreal.log("ARENA_KIT " + str(actor.get_editor_property("player_weapon")))

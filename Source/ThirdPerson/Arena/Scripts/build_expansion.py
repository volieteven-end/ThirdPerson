"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>, with Editor closed."""
import unreal

if not unreal.ArenaAssetTools.build_arena_expansion():
    raise RuntimeError("Arena expansion failed; inspect ARENA_MAP / ARENA_VALIDATE in the log")
unreal.log("ARENA_EXPANSION_COMPLETE")

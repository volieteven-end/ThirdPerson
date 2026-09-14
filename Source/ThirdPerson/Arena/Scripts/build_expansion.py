# 编辑器写入工具：会保存教程／Boss 入口和随机竞技场资源，执行前确认没有未保存的手工编辑。
"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>, with Editor closed."""
import unreal

if not unreal.ArenaAssetTools.build_arena_expansion():
    raise RuntimeError("Arena expansion failed; inspect ARENA_MAP / ARENA_VALIDATE in the log")
unreal.log("ARENA_EXPANSION_COMPLETE")

# 编辑器写入工具：会生成或保存教程场景和课程配置，日常回归不执行此脚本。
"""Run through UE's PythonScript commandlet after building ThirdPersonEditor.

The native builder creates missing tutorial assets only; rerunning preserves the
saved level and any subsequent designer edits. It never saves unrelated packages.
"""
import unreal

if not unreal.TutorialAssetTools.build_forest_tutorial():
    raise RuntimeError('Forest tutorial build or validation failed; inspect the UE log.')
if not unreal.TutorialAssetTools.validate_forest_tutorial():
    raise RuntimeError('Forest tutorial asset validation failed.')
unreal.log('FOREST_TUTORIAL_ASSETS_READY /Game/Third/Tutorial/Maps/L_ForestTutorial')

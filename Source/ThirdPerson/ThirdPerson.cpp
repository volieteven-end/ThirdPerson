
// 注册 ThirdPerson 主游戏模块；编辑器资源工具通过编译条件隔离，游戏运行不构建资产。
#include "ThirdPerson.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE( FDefaultGameModuleImpl, ThirdPerson, "ThirdPerson" );

# 角色动作音效

主角和 `AEnemyCharacter` 派生敌人自动拥有 `TPCCharacterAudioComponent`，不需要在蓝图里重复播放声音。

- 脚步：两个实际移动 BlendSpace 的 25 段动画各有左右脚接地 Notify；只允许最高权重的动画发送 Notify，并过滤切换时的重复通知。蹲行、步行、跑步、冲刺分别选择声音组与音量，频率由动画播放节奏决定。静止、腾空、死亡和战斗动作中不播放普通移动脚步；真实落地单独播放轻重落地声。
- 挥剑：近战攻击窗口开启时播放，按攻击实例和窗口去重；重击使用更厚重的挥砍声，音调跟随蒙太奇播放速度。前摇被打断不会空响。
- 命中：监听 HealthComponent 已结算的结果，敌人掉血播放砍肉声，重击播放肉骨声；主角受伤额外播放痛呼。格挡和弹反使用各自金属声，不混入砍肉与痛呼；无敌、空挥不播放命中声。

在 `BP_TPCCharacter` 的继承组件 `CharacterAudioComponent` 中可调整 `Master Volume`、`Footstep Volume`、`Combat Volume`、`Hurt Voice Volume`，或在 `Sound Banks` 替换各组素材。默认复用项目中 33 个独立 SoundWave，约 2.5 MB，未引入第三方音效生成服务。脚步默认是当前关卡适用的硬地面音色，尚未按地面材质分类。

## 维护与验证

`author_footsteps.py` 通过 Unreal PythonScript commandlet 运行：采样脚掌骨骼的下降接地帧，只更新 `TPC_Footsteps` Notify 轨道，不改原动画和其他轨道。以后改变移动动画时先复核脚步位置。

运行自动化组 `ThirdPerson.CharacterAudio` 检查素材、接地时刻和实际 PIE 行为。实际混音录音使用以下附加参数（仅临时测试进程，不修改项目音量设置）：

```text
-RenderOffscreen -AudioMixer -CharacterAudioRender
-ExecCmds="t.MaxFPS 60,au.DisableAppVolume 1,au.NeverDisableSubmixes 1,au.MuteAudio 1,Automation RunTests ThirdPerson.CharacterAudio;Quit"
-TestExit="Automation Test Queue Empty"
```

自动化环境会静音应用，所以录音测试需临时绕过应用音量，并保留静音期间的 PCM 帧。使用实时音频设备（不加 `-DeterministicAudio`）；`au.MuteAudio` 只静音最终扬声器输出，不影响前面的混音录音。录音与触发记录位于 `Saved/CharacterAudio`。用 `python Source/ThirdPerson/Audio/audit_recordings.py Saved/CharacterAudio` 检查实际录音的有效声音、应静音场景与削波；事件日志本身不等于有声输出。

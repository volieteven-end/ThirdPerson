using UnrealBuildTool;
using System.IO;
public class ForestUIEditor : ModuleRules { public ForestUIEditor(ReadOnlyTargetRules Target) : base(Target) { PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","UMG"}); PrivateDependencyModuleNames.AddRange(new[]{"UnrealEd","UMGEditor","Slate","SlateCore","RenderCore","RHI","Kismet","KismetCompiler","BlueprintGraph","ThirdPerson","Json","JsonUtilities"}); PrivateIncludePaths.Add(Path.Combine(Target.ProjectFile.Directory.FullName,"Source/ThirdPerson")); } }


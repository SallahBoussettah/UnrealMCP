using UnrealBuildTool;
using System.IO;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// UMGEditor's K2Node_CreateWidget.h is in its Private directory
		string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
		PrivateIncludePaths.Add(Path.Combine(EngineDir, "Source", "Editor", "UMGEditor", "Private"));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Sockets",
			"Networking",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"BlueprintGraph",
			"KismetCompiler",
			"GraphEditor",
			"Kismet",
			"AssetRegistry",
			"AssetTools",
			"PropertyEditor",
			"Slate",
			"SlateCore",
			"InputCore",
			"EditorScriptingUtilities",
			"Json",
			"JsonUtilities",
			"ImageWrapper",
			"RenderCore",
			"MaterialEditor",
			"EditorSubsystem",
			"HTTP",
			"EditorFramework",
			"LevelEditor",
			"EnhancedInput",
			"InputBlueprintNodes",
			"UMG",
			"UMGEditor",
			"AnimGraph",
			"AnimGraphRuntime",
			"AudioEditor",
		});
	}
}

using UnrealBuildTool;

public class UnrealMCP : ModuleRules
{
	public UnrealMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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
		});
	}
}

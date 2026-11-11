using UnrealBuildTool;

public class ProtoBoy : ModuleRules
{
	public ProtoBoy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"EditorSubsystem",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"ToolMenus",
			"Projects",
			"Json",
			"JsonUtilities",
			"DesktopPlatform",
		});
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RealityDistortion : ModuleRules
{
	public RealityDistortion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"RenderCore",  // Phase 1: SceneProxy 相关
			"Renderer",    // Phase 2+: MeshPassProcessor
			"RHI"          // Phase 1: FRHIUniformBuffer 等 RHI 资源
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"RealityDistortion",
			"RealityDistortion/Rendering",  // Phase 1: 自定义渲染组件
			"RealityDistortion/Variant_Platforming",
			"RealityDistortion/Variant_Platforming/Animation",
			"RealityDistortion/Variant_Combat",
			"RealityDistortion/Variant_Combat/AI",
			"RealityDistortion/Variant_Combat/Animation",
			"RealityDistortion/Variant_Combat/Gameplay",
			"RealityDistortion/Variant_Combat/Interfaces",
			"RealityDistortion/Variant_Combat/UI",
			"RealityDistortion/Variant_SideScrolling",
			"RealityDistortion/Variant_SideScrolling/AI",
			"RealityDistortion/Variant_SideScrolling/Gameplay",
			"RealityDistortion/Variant_SideScrolling/Interfaces",
			"RealityDistortion/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

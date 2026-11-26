// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class mama : ModuleRules
{
	public mama(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "WebBrowser", "Json", "JsonUtilities", "ApplicationCore" });
		PrivateIncludePaths.AddRange(new string[] { ModuleDirectory, System.IO.Path.Combine(ModuleDirectory, "Private") });
		RuntimeDependencies.Add("$(ProjectDir)/Content/UI/...", StagedFileType.NonUFS);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

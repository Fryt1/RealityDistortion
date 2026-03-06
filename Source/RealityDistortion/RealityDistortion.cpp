// Copyright Epic Games, Inc. All Rights Reserved.

#include "RealityDistortion.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "ShaderCore.h"
#include "HAL/IConsoleManager.h"
#include "RealityDistortionField.h"

DEFINE_LOG_CATEGORY(LogRealityDistortion)

class FRealityDistortionModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// Register virtual shader directory mapping.
		FString ShaderDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/RealityDistortion"), ShaderDirectory);

		// Clear RT-side leftover fields after PIE/hot-reload.
		ResetRealityDistortionFields_GameThread();

		// Keep BasePass depth writes enabled even with full prepass so receiver meshes
		// still write depth outside field coverage when depth pass submission is disabled.
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BasePassWriteDepthEvenWithFullPrepass")))
		{
			CVar->Set(1, ECVF_SetByCode);
		}

		UE_LOG(LogRealityDistortion, Log, TEXT("RealityDistortion module started. Shader directory: %s"), *ShaderDirectory);
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogRealityDistortion, Log, TEXT("RealityDistortion module shutdown."));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRealityDistortionModule, RealityDistortion, "RealityDistortion");


// RealityDistortionShaders.h
//
// Reality Distortion Shader Declarations
// ---------------------------------------
// 定义 Uniform Buffer 和 Shader 类

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "MeshMaterialShader.h"
#include "MeshDrawShaderBindings.h"

// ============================================================================
// Uniform Buffer - 力场参数
// ============================================================================
// 最多支持 4 个力场
#define MAX_DISTORTION_FIELDS 4

// Uniform Buffer 结构体
// 注意：为了避免结构体数组的对齐问题，这里展开为单独的字段
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FRealityDistortionUniformParameters, REALITYDISTORTION_API)
	SHADER_PARAMETER(uint32, ActiveFieldCount)
	SHADER_PARAMETER(float, GlobalDistortionScale)
	SHADER_PARAMETER(float, CurrentTime)
	SHADER_PARAMETER(float, GlitchSpeed)

	// Field 0
	SHADER_PARAMETER(FVector3f, Field0_Center)
	SHADER_PARAMETER(float, Field0_Radius)
	SHADER_PARAMETER(float, Field0_Strength)
	SHADER_PARAMETER(float, Field0_Padding)

	// Field 1
	SHADER_PARAMETER(FVector3f, Field1_Center)
	SHADER_PARAMETER(float, Field1_Radius)
	SHADER_PARAMETER(float, Field1_Strength)
	SHADER_PARAMETER(float, Field1_Padding)

	// Field 2
	SHADER_PARAMETER(FVector3f, Field2_Center)
	SHADER_PARAMETER(float, Field2_Radius)
	SHADER_PARAMETER(float, Field2_Strength)
	SHADER_PARAMETER(float, Field2_Padding)

	// Field 3
	SHADER_PARAMETER(FVector3f, Field3_Center)
	SHADER_PARAMETER(float, Field3_Radius)
	SHADER_PARAMETER(float, Field3_Strength)
	SHADER_PARAMETER(float, Field3_Padding)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// ============================================================================
// 辅助函数 - 构建 Uniform Buffer
// ============================================================================
// 从 RT 侧的力场数据构建 Uniform Buffer
REALITYDISTORTION_API TUniformBufferRef<FRealityDistortionUniformParameters> CreateRealityDistortionUniformBuffer();

// ============================================================================
// Vertex Shader
// ============================================================================
class FRealityDistortionVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRealityDistortionVS, MeshMaterial);

public:
	FRealityDistortionVS() = default;
	FRealityDistortionVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		// 绑定 Uniform Buffer
		RealityDistortionParameters.Bind(Initializer.ParameterMap, TEXT("RealityDistortionParameters"));
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		// 只为不透明和 Masked 材质编译
		return IsOpaqueOrMaskedBlendMode(Parameters.MaterialParameters.BlendMode)
			&& Parameters.MaterialParameters.MaterialDomain == MD_Surface;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("REALITY_DISTORTION_PASS"), 1);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		// 绑定 Uniform Buffer
		TUniformBufferRef<FRealityDistortionUniformParameters> UniformBuffer = CreateRealityDistortionUniformBuffer();
		ShaderBindings.Add(RealityDistortionParameters, UniformBuffer);
	}

private:
	LAYOUT_FIELD(FShaderUniformBufferParameter, RealityDistortionParameters);
};

// ============================================================================
// Pixel Shader
// ============================================================================
class FRealityDistortionPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FRealityDistortionPS, MeshMaterial);

public:
	FRealityDistortionPS() = default;
	FRealityDistortionPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		RealityDistortionParameters.Bind(Initializer.ParameterMap, TEXT("RealityDistortionParameters"));
	}

	static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& Parameters)
	{
		return IsOpaqueOrMaskedBlendMode(Parameters.MaterialParameters.BlendMode)
			&& Parameters.MaterialParameters.MaterialDomain == MD_Surface;
	}

	static void ModifyCompilationEnvironment(const FMaterialShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("REALITY_DISTORTION_PASS"), 1);
	}

	void GetShaderBindings(
		const FScene* Scene,
		ERHIFeatureLevel::Type FeatureLevel,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMaterialRenderProxy& MaterialRenderProxy,
		const FMaterial& Material,
		const FMeshMaterialShaderElementData& ShaderElementData,
		FMeshDrawSingleShaderBindings& ShaderBindings) const
	{
		FMeshMaterialShader::GetShaderBindings(Scene, FeatureLevel, PrimitiveSceneProxy, MaterialRenderProxy, Material, ShaderElementData, ShaderBindings);

		TUniformBufferRef<FRealityDistortionUniformParameters> UniformBuffer = CreateRealityDistortionUniformBuffer();
		ShaderBindings.Add(RealityDistortionParameters, UniformBuffer);
	}

private:
	LAYOUT_FIELD(FShaderUniformBufferParameter, RealityDistortionParameters);
};

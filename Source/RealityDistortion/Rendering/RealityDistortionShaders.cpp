// RealityDistortionShaders.cpp

#include "Rendering/RealityDistortionShaders.h"

#include "HAL/PlatformTime.h"
#include "RealityDistortionField.h"

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FRealityDistortionUniformParameters, "RealityDistortionParameters");

TUniformBufferRef<FRealityDistortionUniformParameters> CreateRealityDistortionUniformBuffer()
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	// Zero initialize to avoid undefined values when some fields are inactive.
	FRealityDistortionUniformParameters Parameters{};

	const TConstArrayView<FRealityDistortionFieldSettings> Fields = GetRealityDistortionFieldSettings_RenderThread();
	Parameters.GlobalDistortionScale = 1.0f;
	Parameters.CurrentTime = static_cast<float>(FPlatformTime::Seconds());
	Parameters.GlitchSpeed = 5.0f;

	auto PackField = [&Parameters](uint32 PackedIndex, const FRealityDistortionFieldSettings& Field)
	{
		switch (PackedIndex)
		{
		case 0:
			Parameters.Field0_Center = FVector3f(Field.Center);
			Parameters.Field0_Radius = Field.Radius;
			Parameters.Field0_Strength = Field.Strength;
			break;
		case 1:
			Parameters.Field1_Center = FVector3f(Field.Center);
			Parameters.Field1_Radius = Field.Radius;
			Parameters.Field1_Strength = Field.Strength;
			break;
		case 2:
			Parameters.Field2_Center = FVector3f(Field.Center);
			Parameters.Field2_Radius = Field.Radius;
			Parameters.Field2_Strength = Field.Strength;
			break;
		case 3:
			Parameters.Field3_Center = FVector3f(Field.Center);
			Parameters.Field3_Radius = Field.Radius;
			Parameters.Field3_Strength = Field.Strength;
			break;
		default:
			break;
		}
	};

	uint32 PackedFieldCount = 0;
	for (const FRealityDistortionFieldSettings& Field : Fields)
	{
		if (!Field.bEnabled || Field.Radius <= 0.0f)
		{
			continue;
		}

		PackField(PackedFieldCount, Field);
		++PackedFieldCount;
		if (PackedFieldCount >= MAX_DISTORTION_FIELDS)
		{
			break;
		}
	}
	Parameters.ActiveFieldCount = PackedFieldCount;

	return TUniformBufferRef<FRealityDistortionUniformParameters>::CreateUniformBufferImmediate(
		Parameters,
		UniformBuffer_SingleFrame);
}

IMPLEMENT_MATERIAL_SHADER_TYPE(
	,
	FRealityDistortionVS,
	TEXT("/Plugin/RealityDistortion/Private/RealityDistortionShader.usf"),
	TEXT("MainVS"),
	SF_Vertex);

IMPLEMENT_MATERIAL_SHADER_TYPE(
	,
	FRealityDistortionPS,
	TEXT("/Plugin/RealityDistortion/Private/RealityDistortionShader.usf"),
	TEXT("MainPS"),
	SF_Pixel);

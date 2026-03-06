// RealityDistortionPassProcessor.cpp

#include "Rendering/RealityDistortionPassProcessor.h"

#include "MaterialShaderType.h"
#include "Materials/Material.h"
#include "MeshPassProcessor.inl"
#include "HAL/IConsoleManager.h"
#include "RealityDistortion.h"
#include "RealityDistortionField.h"
#include "Rendering/DistortionSceneProxy.h"
#include "Rendering/RealityDistortionShaders.h"

namespace
{
	// 任务 3：在 AddMeshBatch 流程里强制修改图元类型，用于观察“线框/点云瓦解”效果。
	static TAutoConsoleVariable<int32> CVarRealityDistortionPrimitiveMode(
		TEXT("r.RealityDistortion.PrimitiveMode"),
		0,
		TEXT("Primitive mode for RealityDistortion pass. 0=TriangleList, 1=LineList, 2=PointList"),
		ECVF_RenderThreadSafe);

	static EPrimitiveType GetRealityDistortionPrimitiveType()
	{
		const int32 Mode = FMath::Clamp(CVarRealityDistortionPrimitiveMode.GetValueOnAnyThread(), 0, 2);
		switch (Mode)
		{
		case 1:
			return PT_LineList;
		case 2:
			return PT_PointList;
		default:
			return PT_TriangleList;
		}
	}
}

FRealityDistortionPassProcessor::FRealityDistortionPassProcessor(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::RealityDistortion, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
	// 使用 Alpha 混合，实现半透明力场球效果
	PassDrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
	// 开启深度测试，但不写入深度，避免覆盖后面的物体
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_DepthNearOrEqual>::GetRHI());
}

void FRealityDistortionPassProcessor::AddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	if (PrimitiveSceneProxy == nullptr)
	{
		return;
	}

	// ==================================================
	// 第一层：Receiver 类型过滤
	// ==================================================
	// 只有自定义 FDistortionSceneProxy 才参与本 Pass。
	// 这样可以把“受影响物体”与普通 BasePass 物体分开。
	if (PrimitiveSceneProxy->GetTypeHash() != FDistortionSceneProxy::GetStaticTypeHash())
	{
		return;
	}

	const FDistortionSceneProxy* DistortionProxy = static_cast<const FDistortionSceneProxy*>(PrimitiveSceneProxy);
	if (!DistortionProxy->ShouldRenderInRealityDistortionPass())
	{
		return;
	}

	// ==================================================
	// 第二层：Field 粗筛（仅空间，不做 Tag）
	// ==================================================
	// 这里保留包围球相交粗筛以减少无意义提交，但不使用 Tag 过滤，
	// 避免与 BasePass 的 receiver 判定条件不一致导致“已挖洞但 RD 没提交”。
	const TConstArrayView<FRealityDistortionFieldSettings> Fields = GetRealityDistortionFieldSettings_RenderThread();
	if (Fields.IsEmpty())
	{
		return;
	}

	const FBoxSphereBounds PrimitiveBounds = PrimitiveSceneProxy->GetBounds();
	const FVector PrimitiveCenter = PrimitiveBounds.Origin;
	const float PrimitiveSphereRadius = FMath::Max(0.0f, PrimitiveBounds.SphereRadius);

	bool bIntersectsAnyPackedField = false;
	uint32 PackedFieldCount = 0;
	for (const FRealityDistortionFieldSettings& Field : Fields)
	{
		if (!Field.bEnabled || Field.Radius <= 0.0f)
		{
			continue;
		}

		++PackedFieldCount;
		if (PackedFieldCount >= MAX_DISTORTION_FIELDS)
		{
			break;
		}
		
		const float IntersectRadius = Field.Radius + PrimitiveSphereRadius;
		const float DistSq = FVector::DistSquared(PrimitiveCenter, Field.Center);
		if (DistSq <= FMath::Square(IntersectRadius))
		{
			bIntersectsAnyPackedField = true;
			break;
		}
	}

	if (!bIntersectsAnyPackedField)
	{
		return;
	}

	if (!MeshBatch.bUseForMaterial)
	{
		return;
	}

	const EPrimitiveType PrimitiveTypeOverride = GetRealityDistortionPrimitiveType();
	const FMeshBatch* EffectiveMeshBatch = &MeshBatch;
	FMeshBatch OverriddenMeshBatch;

	if (PrimitiveTypeOverride != MeshBatch.Type)
	{
		OverriddenMeshBatch = MeshBatch;
		OverriddenMeshBatch.Type = PrimitiveTypeOverride;

		// 源数据来自三角形网格。切到线/点时，按索引数量重算 NumPrimitives，避免非法读索引。
		for (FMeshBatchElement& BatchElement : OverriddenMeshBatch.Elements)
		{
			const uint32 SourceIndexCount = BatchElement.NumPrimitives * 3u;
			if (PrimitiveTypeOverride == PT_LineList)
			{
				BatchElement.NumPrimitives = SourceIndexCount / 2u;
			}
			else if (PrimitiveTypeOverride == PT_PointList)
			{
				BatchElement.NumPrimitives = SourceIndexCount;
			}
		}

		EffectiveMeshBatch = &OverriddenMeshBatch;
	}

	// ==================================================
	// 第三层：材质 fallback 链
	// ==================================================
	// 同 BasePass 思路：沿 MaterialRenderProxy->Fallback 链找可用 ShaderMap。
	bool bSubmitted = false;
	const FMaterialRenderProxy* MaterialRenderProxy = EffectiveMeshBatch->MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material && Material->GetRenderingThreadShaderMap())
		{
			if (TryAddMeshBatch(*EffectiveMeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
			{
				bSubmitted = true;
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
	}

	// 某些工程材质不会为自定义 pass 编译对应 shader，兜底到默认 Surface 材质，
	// 避免 BasePass 已 clip 但 Distortion pass 无法提交 DrawCommand 导致黑洞。
	if (!bSubmitted)
	{
		if (UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			const FMaterialRenderProxy* DefaultProxy = DefaultMaterial->GetRenderProxy();
			const FMaterial* DefaultMat = DefaultProxy ? DefaultProxy->GetMaterialNoFallback(FeatureLevel) : nullptr;
			if (DefaultMat && DefaultMat->GetRenderingThreadShaderMap())
			{
				TryAddMeshBatch(*EffectiveMeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *DefaultProxy, *DefaultMat);
			}
		}
	}
}

bool FRealityDistortionPassProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// 只处理不透明/Masked；半透明直接跳过。
	const EBlendMode BlendMode = Material.GetBlendMode();
	if (IsTranslucentBlendMode(BlendMode))
	{
		return true;
	}

	// Volume 材质域不在本 Pass 渲染。
	if (Material.GetMaterialDomain() == MD_Volume)
	{
		return true;
	}

	const FMaterialRenderProxy* FinalMaterialProxy = &MaterialRenderProxy;
	const FMaterial* FinalMaterial = &Material;

	// 计算 Fill/Cull 状态，准备进入 Process 构建 DrawCommand。
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(*FinalMaterial, OverrideSettings);
	// Force two-sided in this pass so inside/outside camera positions render consistently.
	const ERasterizerCullMode MeshCullMode = CM_None;

	return Process(
		MeshBatch,
		BatchElementMask,
		StaticMeshId,
		PrimitiveSceneProxy,
		*FinalMaterialProxy,
		*FinalMaterial,
		MeshFillMode,
		MeshCullMode);
}

bool FRealityDistortionPassProcessor::Process(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	int32 StaticMeshId,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
	const FMaterial& RESTRICT MaterialResource,
	ERasterizerFillMode MeshFillMode,
	ERasterizerCullMode MeshCullMode)
{
	const FVertexFactory* VertexFactory = MeshBatch.VertexFactory;

	// RealityDistortion Pass 必须始终使用自定义 PS（输出青色），不能跳过。
	// 构建 Shader 类型列表
	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType<FRealityDistortionVS>();
	ShaderTypes.AddShaderType<FRealityDistortionPS>();

	// 获取编译好的 Shader
	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		UE_LOG(LogRealityDistortion, Warning, TEXT("[RealityDistortion] TryGetShaders FAILED for material: %s, VF: %s"),
			*MaterialResource.GetFriendlyName(), VertexFactory->GetType()->GetName());
		return false;
	}

	// 提取 VS 和 PS
	TMeshProcessorShaders<FRealityDistortionVS, FRealityDistortionPS> PassShaders;
	Shaders.TryGetVertexShader(PassShaders.VertexShader);
	Shaders.TryGetPixelShader(PassShaders.PixelShader);

	// ShaderElementData 会把 Primitive/Material 相关绑定数据带到 DrawCommand。
	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	// 最终生成 FMeshDrawCommand（包含 PSO、Shader、VertexStreams、Bindings）。
	BuildMeshDrawCommands(
		MeshBatch,
		BatchElementMask,
		PrimitiveSceneProxy,
		MaterialRenderProxy,
		MaterialResource,
		PassDrawRenderState,
		PassShaders,
		MeshFillMode,
		MeshCullMode,
		SortKey,
		EMeshPassFeatures::Default,
		ShaderElementData);

	return true;
}

static FMeshPassProcessor* CreateRealityDistortionPassProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
{
	return new FRealityDistortionPassProcessor(Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext);
}

// 注册到 Deferred + MainView。
// 是否缓存静态命令由 SceneVisibility.cpp 的 AddCommandsForMesh(bCanCache) 决定。
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(
	RealityDistortionPass,
	CreateRealityDistortionPassProcessor,
	EShadingPath::Deferred,
	EMeshPass::RealityDistortion,
	EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);

// RealityDistortionPassProcessor.cpp

#include "Rendering/RealityDistortionPassProcessor.h"

#include "MaterialShaderType.h"
#include "Materials/Material.h"
#include "MeshPassProcessor.inl"
#include "RealityDistortionField.h"
#include "Rendering/DistortionSceneProxy.h"

FRealityDistortionPassProcessor::FRealityDistortionPassProcessor(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
	: FMeshPassProcessor(EMeshPass::RealityDistortion, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
{
	// 本 Pass 采用“类似深度 Pass”的基础状态：不混合，深度可写。
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
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
	// 第二层：Field 过滤（Tag + 空间）
	// ==================================================
	// 读取 RT 当前所有 Field，命中任意一个启用 Field 才继续。
	const TConstArrayView<FRealityDistortionFieldSettings> Fields = GetRealityDistortionFieldSettings_RenderThread();
	if (Fields.IsEmpty())
	{
		return;
	}

	const FVector PrimitiveOrigin = PrimitiveSceneProxy->GetBounds().Origin;
	bool bInsideAnyField = false;
	for (const FRealityDistortionFieldSettings& Field : Fields)
	{
		if (!Field.bEnabled || Field.Radius <= 0.0f)
		{
			continue;
		}

		// Field 指定了 Tag 时，只影响标签命中的接收体。
		if (!DistortionProxy->HasReceiverTag(Field.ReceiverTagFilter))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(PrimitiveOrigin, Field.Center);
		if (DistSq <= FMath::Square(Field.Radius))
		{
			bInsideAnyField = true;
			break;
		}
	}

	if (!bInsideAnyField)
	{
		return;
	}

	if (!MeshBatch.bUseForMaterial)
	{
		return;
	}

	// ==================================================
	// 第三层：材质 fallback 链
	// ==================================================
	// 同 BasePass 思路：沿 MaterialRenderProxy->Fallback 链找可用 ShaderMap。
	const FMaterialRenderProxy* MaterialRenderProxy = MeshBatch.MaterialRenderProxy;
	while (MaterialRenderProxy)
	{
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);
		if (Material && Material->GetRenderingThreadShaderMap())
		{
			if (TryAddMeshBatch(MeshBatch, BatchElementMask, PrimitiveSceneProxy, StaticMeshId, *MaterialRenderProxy, *Material))
			{
				break;
			}
		}

		MaterialRenderProxy = MaterialRenderProxy->GetFallback(FeatureLevel);
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

	// DepthOnly shader 对“普通不透明材质”常常没有可用 permutation。
	// 这里沿用引擎深度 Pass 的做法：必要时回退到 DefaultMaterial。
	const bool bNeedsDefaultMaterial = Material.WritesEveryPixel(false, false)
		&& !Material.MaterialUsesPixelDepthOffset_RenderThread()
		&& !Material.MaterialMayModifyMeshPosition();

	if (bNeedsDefaultMaterial)
	{
		const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		const FMaterial* DefaultMaterial = DefaultProxy->GetMaterialNoFallback(FeatureLevel);
		if (DefaultMaterial && DefaultMaterial->GetRenderingThreadShaderMap())
		{
			FinalMaterialProxy = DefaultProxy;
			FinalMaterial = DefaultMaterial;
		}
	}

	// 计算 Fill/Cull 状态，准备进入 Process 构建 DrawCommand。
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(*FinalMaterial, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(*FinalMaterial, OverrideSettings);

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

	// 运行时按名称查询 Depth Pass 的 shader/pipeline。
	// 这样可以避免直接依赖 Renderer 私有模板类型定义。
	static FShaderType* DepthVSType = FShaderType::GetShaderTypeByName(TEXT("TDepthOnlyVS<false>"));
	static FShaderType* DepthPSType = FShaderType::GetShaderTypeByName(TEXT("FDepthOnlyPS"));
	static const FShaderPipelineType* DepthNoPixelPipeline =
		FShaderPipelineType::GetShaderPipelineTypeByName(FHashedName(TEXT("DepthNoPixelPipeline")));
	static const FShaderPipelineType* DepthWithPixelPipeline =
		FShaderPipelineType::GetShaderPipelineTypeByName(FHashedName(TEXT("DepthPipeline")));

	if (!DepthVSType)
	{
		return false;
	}

	// 只有“不写满像素”或使用 PixelDepthOffset 的材质才需要 PS。
	const bool bVFTypeSupportsNullPixelShader = VertexFactory->GetType()->SupportsNullPixelShader();
	const bool bNeedsPixelShader = !MaterialResource.WritesEveryPixel(false, bVFTypeSupportsNullPixelShader)
		|| MaterialResource.MaterialUsesPixelDepthOffset_RenderThread();

	FMaterialShaderTypes ShaderTypes;
	ShaderTypes.AddShaderType(DepthVSType);
	if (bNeedsPixelShader && DepthPSType)
	{
		ShaderTypes.AddShaderType(DepthPSType);
		ShaderTypes.PipelineType = DepthWithPixelPipeline;
	}
	else
	{
		ShaderTypes.PipelineType = DepthNoPixelPipeline;
	}

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		return false;
	}

	TMeshProcessorShaders<FMeshMaterialShader, FMeshMaterialShader> PassShaders;
	Shaders.TryGetShader(SF_Vertex, PassShaders.VertexShader);
	if (bNeedsPixelShader)
	{
		Shaders.TryGetShader(SF_Pixel, PassShaders.PixelShader);
	}

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

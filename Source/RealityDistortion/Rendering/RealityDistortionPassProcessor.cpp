// RealityDistortionPassProcessor.cpp
// Phase 2 实战任务：自定义 MeshPassProcessor - 核心决策逻辑

#include "Rendering/RealityDistortionPassProcessor.h"
#include "MeshPassProcessor.inl"
#include "Materials/Material.h"
#include "MaterialShaderType.h"

// ========================================
// 构造函数
// ========================================
FRealityDistortionPassProcessor::FRealityDistortionPassProcessor(
	const FScene* Scene,
	ERHIFeatureLevel::Type FeatureLevel,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext,
	const FVector& InFieldCenter,
	float InFieldRadius)
	: FMeshPassProcessor(EMeshPass::RealityDistortion, Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext)
	, FieldCenter(InFieldCenter)
	, FieldRadius(InFieldRadius)
{
	// 配置渲染状态（同 CustomDepthPass：写入深度，不混合）
	PassDrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
	PassDrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI());
}

// ========================================
// 第一层决策：AddMeshBatch - 空间过滤 + 材质解析
// ========================================
void FRealityDistortionPassProcessor::AddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId)
{
	// ---- 空间过滤：距离判断 ----
	// 这是我们"决策层"的核心：只处理 FieldCenter 附近的物体
	if (PrimitiveSceneProxy)
	{
		const FVector PrimitiveOrigin = PrimitiveSceneProxy->GetBounds().Origin;
		const float DistSq = FVector::DistSquared(PrimitiveOrigin, FieldCenter);
		const float Dist = FMath::Sqrt(DistSq);
		
		if (DistSq > FMath::Square(FieldRadius))
		{
			UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] SKIP %s (dist=%.0f > radius=%.0f)"),
				*PrimitiveSceneProxy->GetOwnerName().ToString(), Dist, FieldRadius);
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] ACCEPT %s (dist=%.0f <= radius=%.0f)"),
			*PrimitiveSceneProxy->GetOwnerName().ToString(), Dist, FieldRadius);
	}

	// ---- 材质解析：Fallback 链遍历 ----
	// 同 BasePass/CustomDepth 模式：沿 Fallback 链找到第一个有效材质
	if (MeshBatch.bUseForMaterial)
	{
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
}

// ========================================
// 第二层决策：TryAddMeshBatch - BlendMode 过滤
// ========================================
bool FRealityDistortionPassProcessor::TryAddMeshBatch(
	const FMeshBatch& RESTRICT MeshBatch,
	uint64 BatchElementMask,
	const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
	int32 StaticMeshId,
	const FMaterialRenderProxy& MaterialRenderProxy,
	const FMaterial& Material)
{
	// 只处理不透明和 Masked 材质（跳过半透明）
	const EBlendMode BlendMode = Material.GetBlendMode();
	const bool bIsTranslucent = IsTranslucentBlendMode(BlendMode);
	
	if (bIsTranslucent)
	{
		return true; // 半透明材质不画，但返回 true 表示已处理
	}

	// 检查材质域（内联 ScenePrivate.h 中的 ShouldIncludeDomainInMeshPass）
	// Volume 域材质只在体素化 Pass 中渲染
	if (Material.GetMaterialDomain() == MD_Volume)
	{
		return true;
	}

	// ★ 关键：对不透明材质使用 DefaultMaterial
	// TDepthOnlyVS<false> 的 ShouldCompilePermutation 只对以下情况返回 true：
	//   1. bIsSpecialEngineMaterial（引擎默认材质）
	//   2. !bWritesEveryPixel（Masked 材质）
	//   3. bMaterialMayModifyMeshPosition（WPO 材质）
	// 普通不透明材质不满足这些条件，Shader 没有被编译。
	// 引擎 DepthPass 的做法是：对普通不透明物体替换为 DefaultMaterial。
	const FMaterialRenderProxy* FinalMaterialProxy = &MaterialRenderProxy;
	const FMaterial* FinalMaterial = &Material;

	const bool bNeedsDefaultMaterial = Material.WritesEveryPixel(false, false)
		&& !Material.MaterialUsesPixelDepthOffset_RenderThread()
		&& !Material.MaterialMayModifyMeshPosition();

	const FMaterialRenderProxy* DefaultProxy = nullptr;
	if (bNeedsDefaultMaterial)
	{
		DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		const FMaterial* DefaultMat = DefaultProxy->GetMaterialNoFallback(FeatureLevel);
		UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] TryAddMeshBatch: bNeedsDefaultMaterial=1, DefaultMat=%p, HasShaderMap=%d"),
			DefaultMat, DefaultMat ? (DefaultMat->GetRenderingThreadShaderMap() != nullptr) : 0);
		if (DefaultMat && DefaultMat->GetRenderingThreadShaderMap())
		{
			FinalMaterialProxy = DefaultProxy;
			FinalMaterial = DefaultMat;
			UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] TryAddMeshBatch: Switched to DefaultMaterial!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] TryAddMeshBatch: bNeedsDefaultMaterial=0 (Masked/WPO material)"));
	}

	// 计算光栅化模式
	const FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(MeshBatch);
	const ERasterizerFillMode MeshFillMode = ComputeMeshFillMode(*FinalMaterial, OverrideSettings);
	const ERasterizerCullMode MeshCullMode = ComputeMeshCullMode(*FinalMaterial, OverrideSettings);

	return Process(MeshBatch, BatchElementMask, StaticMeshId, PrimitiveSceneProxy,
		*FinalMaterialProxy, *FinalMaterial, MeshFillMode, MeshCullMode);
}

// ========================================
// 第三层决策：Process - 获取 Shader → 配置 PSO → BuildMeshDrawCommands
// ========================================
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

	// --- 运行时查找 Shader 类型 和 Pipeline 类型 ---
	// ★ 关键修复：DepthOnly shader 只在 Pipeline 中编译（ShouldOptimizeUnusedOutputs=true）
	//   不设置 PipelineType 会导致 TryGetShaders 走独立 shader 路径，那里找不到 shader。
	static FShaderType* DepthVSType = FShaderType::GetShaderTypeByName(TEXT("TDepthOnlyVS<false>"));
	static FShaderType* DepthPSType = FShaderType::GetShaderTypeByName(TEXT("FDepthOnlyPS"));
	static const FShaderPipelineType* DepthNoPixelPipeline = FShaderPipelineType::GetShaderPipelineTypeByName(FHashedName(TEXT("DepthNoPixelPipeline")));
	static const FShaderPipelineType* DepthWithPixelPipeline = FShaderPipelineType::GetShaderPipelineTypeByName(FHashedName(TEXT("DepthPipeline")));

	if (!DepthVSType)
	{
		UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] Process FAIL: VS ShaderType not found!"));
		return false;
	}

	// 模仿引擎 GetDepthPassShaders 的逻辑：
	// 只有 Masked 材质（不写入每个像素）才需要 PixelShader
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

	UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] Process: VF=%s NeedsPS=%d Pipeline=%s"),
		VertexFactory->GetType()->GetName(), bNeedsPixelShader,
		ShaderTypes.PipelineType ? ShaderTypes.PipelineType->GetName() : TEXT("NULL"));

	FMaterialShaders Shaders;
	if (!MaterialResource.TryGetShaders(ShaderTypes, VertexFactory->GetType(), Shaders))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] Process FAIL: TryGetShaders failed for VF=%s NeedsPS=%d IsSpecialEngine=%d Pipeline=%s"),
			VertexFactory->GetType()->GetName(), bNeedsPixelShader,
			MaterialResource.IsSpecialEngineMaterial(),
			ShaderTypes.PipelineType ? ShaderTypes.PipelineType->GetName() : TEXT("NULL"));
		return false;
	}
	
	UE_LOG(LogTemp, Warning, TEXT("[RealityDistortion] Process SUCCESS: TryGetShaders passed!"));

	TMeshProcessorShaders<FMeshMaterialShader, FMeshMaterialShader> PassShaders;
	Shaders.TryGetShader(SF_Vertex, PassShaders.VertexShader);
	if (bNeedsPixelShader)
	{
		Shaders.TryGetShader(SF_Pixel, PassShaders.PixelShader);
	}

	// 初始化 ShaderElementData
	FMeshMaterialShaderElementData ShaderElementData;
	ShaderElementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, PrimitiveSceneProxy, MeshBatch, StaticMeshId, false);

	// 排序键
	const FMeshDrawCommandSortKey SortKey = CalculateMeshStaticSortKey(PassShaders.VertexShader, PassShaders.PixelShader);

	// 最终调用：生成 FMeshDrawCommand
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

// ========================================
// 工厂函数 + 注册
// ========================================
FMeshPassProcessor* CreateRealityDistortionPassProcessor(
	ERHIFeatureLevel::Type FeatureLevel,
	const FScene* Scene,
	const FSceneView* InViewIfDynamicMeshCommand,
	FMeshPassDrawListContext* InDrawListContext)
{
	// 工厂创建时使用默认值（引擎在 static draw command 缓存时调用）
	// 实际的 FieldCenter/FieldRadius 会在动态路径中通过 View 传入
	return new FRealityDistortionPassProcessor(
		Scene, FeatureLevel, InViewIfDynamicMeshCommand, InDrawListContext,
		FVector::ZeroVector, 500.0f);
}

// 向引擎注册：Deferred 路径 + MainView + CachedMeshCommands（静态网格走缓存路径）
REGISTER_MESHPASSPROCESSOR_AND_PSOCOLLECTOR(
	RealityDistortionPass,
	CreateRealityDistortionPassProcessor,
	EShadingPath::Deferred,
	EMeshPass::RealityDistortion,
	EMeshPassFlags::CachedMeshCommands | EMeshPassFlags::MainView);

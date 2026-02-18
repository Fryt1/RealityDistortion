// DistortionSceneProxy.cpp
// Phase 1 实战任务：数据劫持 - 核心劫持逻辑

#include "Rendering/DistortionSceneProxy.h"
#include "Rendering/DistortionMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "SceneManagement.h"

FDistortionSceneProxy::FDistortionSceneProxy(UDistortionMeshComponent* InComponent)
	: FStaticMeshSceneProxy(InComponent, false)  // false = 不使用 LOD 过渡
	, OverrideMaterialProxy(nullptr)
{
	// 调试：确认构造函数被调用
	UE_LOG(LogTemp, Warning, TEXT("[DistortionProxy] Constructor called!"));
	
#if WITH_EDITOR
	// 将 OverrideMaterial 加入验证列表（正确的做法）
	// SetUsedMaterialForVerification 是 FPrimitiveSceneProxy 提供的公开 API
	if (InComponent->OverrideMaterial)
	{
		TArray<UMaterialInterface*> UsedMats;
		InComponent->GetUsedMaterials(UsedMats);
		UsedMats.AddUnique(InComponent->OverrideMaterial);
		SetUsedMaterialForVerification(UsedMats);
	}
#endif
	
	// ========================================
	// 关键点：从 GameThread 拷贝数据到 RenderThread
	// ========================================
	// 构造函数在 GameThread 执行
	// 把材质的 RenderProxy 缓存下来供 RenderThread 使用
	
	if (InComponent->OverrideMaterial)
	{
		// GetRenderProxy() 返回的指针是线程安全的
		OverrideMaterialProxy = InComponent->OverrideMaterial->GetRenderProxy();
		UE_LOG(LogTemp, Warning, TEXT("[DistortionProxy] OverrideMaterial found, proxy cached!"));
	}
	else
	{
		// 没有指定材质时，使用引擎默认材质
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		if (DefaultMaterial)
		{
			OverrideMaterialProxy = DefaultMaterial->GetRenderProxy();
			UE_LOG(LogTemp, Warning, TEXT("[DistortionProxy] Using default material!"));
		}
	}
}

FDistortionSceneProxy::~FDistortionSceneProxy()
{
	// FMaterialRenderProxy 由引擎管理，不需要手动释放
}

SIZE_T FDistortionSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FPrimitiveViewRelevance FDistortionSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	// 获取父类的视图相关性
	FPrimitiveViewRelevance Result = FStaticMeshSceneProxy::GetViewRelevance(View);
	
	// 强制使用动态绘制路径！
	// 这是关键：设置 bDynamicRelevance = true 会让渲染器调用 GetDynamicMeshElements
	// 而不是使用缓存的静态绘制指令
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;  // 禁用静态路径
	
	return Result;
}

void FDistortionSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	// 调试日志已移除（每帧调用，会刷屏）
	
	// ========================================
	// 劫持策略：重写整个逻辑，自己填充 MeshBatch
	// ========================================
	
	// 如果没有覆盖材质，回退到父类行为
	if (OverrideMaterialProxy == nullptr)
	{
		FStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		return;
	}

	// 使用父类的 RenderData 成员（已在构造函数中初始化）
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return;
	}

	// 遍历每个 View（摄像机视角）
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];
		
		// 检查可见性位图
		if (!(VisibilityMap & (1 << ViewIndex)))
		{
			continue;
		}

		// 使用 LOD 0（最高精度）简化实现
		const int32 LODIndex = 0;
		const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

		// 遍历每个 Section（材质槽）
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

			// 从 Collector 分配空的 MeshBatch
			FMeshBatch& MeshBatch = Collector.AllocateMesh();

			// ========================================
			// 填充 MeshBatch
			// ========================================
			
			// 1. 顶点工厂 - 告诉 GPU 顶点数据在哪里
			MeshBatch.VertexFactory = &RenderData->LODVertexFactories[LODIndex].VertexFactory;
			
			// 2. 劫持点！用我们的材质替换原本的材质
			MeshBatch.MaterialRenderProxy = OverrideMaterialProxy;
			
			// 3. 场景代理基本信息
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.DepthPriorityGroup = SDPG_World;
			MeshBatch.LODIndex = LODIndex;
			MeshBatch.bCanApplyViewModeOverrides = true;
			MeshBatch.CastShadow = true;
			
			// 4. 索引缓冲信息
			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.IndexBuffer = &LODModel.IndexBuffer;
			BatchElement.FirstIndex = Section.FirstIndex;
			BatchElement.NumPrimitives = Section.NumTriangles;
			BatchElement.MinVertexIndex = Section.MinVertexIndex;
			BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
			
			// 5. Primitive Uniform Buffer（变换矩阵等）
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

			// 提交 MeshBatch 到 Collector
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}

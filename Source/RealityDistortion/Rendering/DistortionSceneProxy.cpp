// DistortionSceneProxy.cpp

#include "Rendering/DistortionSceneProxy.h"

#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "Rendering/DistortionMeshComponent.h"
#include "SceneManagement.h"

FDistortionSceneProxy::FDistortionSceneProxy(UDistortionMeshComponent* InComponent)
	: FStaticMeshSceneProxy(InComponent, false)
{
#if WITH_EDITOR
	// 编辑器验证：把 OverrideMaterial 加入 UsedMats，避免材质引用检查误报。
	if (InComponent->OverrideMaterial)
	{
		TArray<UMaterialInterface*> UsedMats;
		InComponent->GetUsedMaterials(UsedMats);
		UsedMats.AddUnique(InComponent->OverrideMaterial);
		SetUsedMaterialForVerification(UsedMats);
	}
#endif

	// 构造函数在 GT 执行，这里把 Receiver 配置拷贝到 Proxy 的纯数据字段。
	// 后续 RT 不再访问 UDistortionMeshComponent，避免跨线程访问 UObject。
	bEnableDistortionReceiver = InComponent->bEnableDistortionReceiver;

	// 收集接收体标签（组件 + Actor），供 Field 在 RT 按 Tag 过滤。
	for (const FName& ComponentTag : InComponent->ComponentTags)
	{
		if (!ComponentTag.IsNone())
		{
			ReceiverTags.AddUnique(ComponentTag);
		}
	}

	if (const AActor* OwnerActor = InComponent->GetOwner())
	{
		for (const FName& ActorTag : OwnerActor->Tags)
		{
			if (!ActorTag.IsNone())
			{
				ReceiverTags.AddUnique(ActorTag);
			}
		}
	}

	if (InComponent->OverrideMaterial)
	{
		// 缓存 RenderProxy，RT 直接使用。
		OverrideMaterialProxy = InComponent->OverrideMaterial->GetRenderProxy();
	}
	else
	{
		// 没有覆盖材质时回退默认 Surface 材质。
		if (UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
		{
			OverrideMaterialProxy = DefaultMaterial->GetRenderProxy();
		}
	}
}

FDistortionSceneProxy::~FDistortionSceneProxy()
{
}

SIZE_T FDistortionSceneProxy::GetStaticTypeHash()
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

SIZE_T FDistortionSceneProxy::GetTypeHash() const
{
	return GetStaticTypeHash();
}

FPrimitiveViewRelevance FDistortionSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result = FStaticMeshSceneProxy::GetViewRelevance(View);

	// 强制走动态路径：每帧都会执行 GetDynamicMeshElements，材质劫持可实时生效。
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;

	return Result;
}

void FDistortionSceneProxy::GetDynamicMeshElements(
	const TArray<const FSceneView*>& Views,
	const FSceneViewFamily& ViewFamily,
	uint32 VisibilityMap,
	FMeshElementCollector& Collector) const
{
	// 没有覆盖材质时，回退父类逻辑。
	if (OverrideMaterialProxy == nullptr)
	{
		FStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		return;
	}

	// 没有可用的 RenderData 时直接退出。
	if (RenderData == nullptr || RenderData->LODResources.Num() == 0)
	{
		return;
	}

	// ==============================
	// 遍历每个可见 View（摄像机）
	// ==============================
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		// 可见性位图过滤：当前 View 不可见则跳过。
		if ((VisibilityMap & (1 << ViewIndex)) == 0)
		{
			continue;
		}

		// 这里固定使用 LOD0，便于教学与调试；后续可按距离切换 LOD。
		const int32 LODIndex = 0;
		const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

		// 遍历 LOD 的每个 Section（材质槽）。
		for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

			// 向 Collector 申请一个新的 MeshBatch。
			FMeshBatch& MeshBatch = Collector.AllocateMesh();

			// ----------------------------------------
			// 1) VertexFactory + Material
			// ----------------------------------------
			// VertexFactory 决定顶点流如何绑定和解释。
			MeshBatch.VertexFactory = &RenderData->LODVertexFactories[LODIndex].VertexFactory;
			// 劫持点：把原材质替换成 OverrideMaterialProxy。
			MeshBatch.MaterialRenderProxy = OverrideMaterialProxy;

			// ----------------------------------------
			// 2) 基础绘制状态
			// ----------------------------------------
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.Type = PT_TriangleList;
			MeshBatch.DepthPriorityGroup = SDPG_World;
			MeshBatch.LODIndex = LODIndex;
			MeshBatch.bCanApplyViewModeOverrides = true;
			MeshBatch.CastShadow = true;

			// ----------------------------------------
			// 3) Section 索引范围
			// ----------------------------------------
			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.IndexBuffer = &LODModel.IndexBuffer;
			BatchElement.FirstIndex = Section.FirstIndex;
			BatchElement.NumPrimitives = Section.NumTriangles;
			BatchElement.MinVertexIndex = Section.MinVertexIndex;
			BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
			// PrimitiveUniformBuffer 提供 LocalToWorld 等每个 Primitive 的常量数据。
			BatchElement.PrimitiveUniformBuffer = GetUniformBuffer();

			// 最终提交给 Collector，后续进入 MeshPassProcessor 的 AddMeshBatch。
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}

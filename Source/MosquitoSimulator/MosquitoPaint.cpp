// Copyright your name. All Rights Reserved.

#include "MosquitoPaint.h"

#include "Components/PrimitiveComponent.h"
#include "MaterialExpressionIO.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/Package.h"

namespace
{
	/** Vector parameter baked into the generated parent material. */
	constexpr TCHAR PaintParamName[] = TEXT("MosquitoColor");

	/**
	 * QA MVP paint fix: placeholder meshes use engine materials that expose no
	 * usable color vector parameter (PIE showed the WorldGridMaterial
	 * checkerboard), so SetVectorParameterValue on a MID parented to them is a
	 * silent no-op - the value is stashed in the instance while the shader
	 * keeps rendering the parent. The old MID-level readback "proof" passed
	 * because an instance-level lookup finds stashed parameters.
	 *
	 * Fix: build one shared parent material at runtime with a VectorParameter
	 * genuinely wired into the shader graph (EmissiveColor + BaseColor), unlit
	 * so painted colors stay readable at night exposure. Rooted for the whole
	 * session; all MIDs share it, one shader compile total.
	 *
	 * @param bOutParamConnected - true only if the parent's published parameter
	 *        collection exposes PaintParamName (honest probe, editor-only path).
	 * @return the rooted parent material, or nullptr when the probe failed.
	 */
	UMaterial* GetSharedParentMaterial(bool& bOutParamConnected)
	{
		static UMaterial* Cached = nullptr;
		bOutParamConnected = false;

		if (Cached)
		{
			bOutParamConnected = true; // rooted, verified once on creation
			return Cached;
		}

		UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);
		Material->MaterialDomain = MD_Surface;
		Material->SetShadingModel(MSM_Unlit);

		UMaterialExpressionVectorParameter* Param = NewObject<UMaterialExpressionVectorParameter>(Material);
		Param->ParameterName = PaintParamName;
		Param->DefaultValue = FLinearColor::White;
		Material->GetExpressionCollection().AddExpression(Param);

		// Wire the parameter into the shader graph for real (editor-only API).
#if WITH_EDITOR
		if (FExpressionInput* EmissiveInput = Material->GetExpressionInputForProperty(MP_EmissiveColor))
		{
			EmissiveInput->Connect(0, Param);
		}
		if (FExpressionInput* BaseColorInput = Material->GetExpressionInputForProperty(MP_BaseColor))
		{
			BaseColorInput->Connect(0, Param);
		}
		Material->PostEditChange(); // publish parameter collection + compile
#endif

		// Honest proof: the parameter must come from the PARENT's published
		// collection, not from an instance stash (that was the false positive).
		FLinearColor Probe(ForceInit);
		bOutParamConnected = Material->GetVectorParameterValue(FMaterialParameterInfo(PaintParamName), Probe);

		UE_LOG(LogTemp, Log, TEXT("[MosquitoPaint] parent '%s' built: param '%s' connected=%s"),
			*Material->GetName(), PaintParamName, bOutParamConnected ? TEXT("OK") : TEXT("FAIL"));

		if (bOutParamConnected)
		{
			Material->AddToRoot();
			Cached = Material;
		}
		else
		{
			Material->MarkAsGarbage();
		}

		return Cached;
	}
}

namespace MosquitoPaint
{
	void PaintMesh(UPrimitiveComponent* Mesh, const FLinearColor& Color)
	{
		if (!Mesh)
		{
			return;
		}

		bool bParamConnected = false;
		UMaterial* Parent = GetSharedParentMaterial(bParamConnected);
		if (!Parent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MosquitoPaint] %s: no verified parent material, mesh left unpainted"),
				*Mesh->GetFName().ToString());
			return;
		}

		// Explicit parent: bypasses whatever (parameter-less) material sits in
		// the mesh's slot 0, so the parameter is guaranteed to reach the shader.
		UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Parent);
		if (!MID)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MosquitoPaint] %s: MID creation failed"), *Mesh->GetFName().ToString());
			return;
		}

		MID->SetVectorParameterValue(PaintParamName, Color);

		// QA: instance-level readback now reflects a real shader override.
		FLinearColor ReadBack(ForceInit);
		const bool bRead = MID->GetVectorParameterValue(FMaterialParameterInfo(PaintParamName), ReadBack);
		UE_LOG(LogTemp, Log, TEXT("[MosquitoPaint] %s: parent=%s param=%s set=(%.2f,%.2f,%.2f) readback=%s (%.2f,%.2f,%.2f)"),
			*Mesh->GetFName().ToString(), *Parent->GetName(),
			bParamConnected ? TEXT("CONNECTED") : TEXT("FALLBACK"),
			Color.R, Color.G, Color.B,
			bRead ? TEXT("OK") : TEXT("FAIL"), ReadBack.R, ReadBack.G, ReadBack.B);
	}
}

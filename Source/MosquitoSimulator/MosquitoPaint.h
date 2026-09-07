// Copyright your name. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UPrimitiveComponent;

/** QA MVP: shared runtime painter for placeholder engine primitives. */
namespace MosquitoPaint
{
	/** Creates a MID on slot 0 and sets the material's color vector parameter. */
	MOSQUITOSIMULATOR_API void PaintMesh(UPrimitiveComponent* Mesh, const FLinearColor& Color);
}

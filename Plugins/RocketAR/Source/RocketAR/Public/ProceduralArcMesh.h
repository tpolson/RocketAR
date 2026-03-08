#pragma once

#include "CoreMinimal.h"
#include "ProceduralArcMesh.generated.h"

/**
 * Generates vertices, UVs, and triangles for a cylindrical arc mesh.
 * Used for banner text rendering on a curved surface.
 */
UCLASS(BlueprintType)
class ROCKETAR_API UProceduralArcMesh : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Generate a cylindrical arc mesh.
	 * @param ArcAngleDegrees Total arc angle (e.g., 120 degrees)
	 * @param Radius Arc radius in cm
	 * @param Height Arc height in cm
	 * @param Segments Number of segments along the arc (higher = smoother)
	 * @param OutVertices Output vertex positions
	 * @param OutUVs Output UV coordinates (U maps along arc, V maps along height)
	 * @param OutTriangles Output triangle indices
	 * @param OutNormals Output vertex normals (pointing inward toward center)
	 * @param OutTangents Output vertex tangents
	 */
	UFUNCTION(BlueprintCallable, Category = "Procedural Mesh")
	static void GenerateArcMesh(
		float ArcAngleDegrees,
		float Radius,
		float Height,
		int32 Segments,
		TArray<FVector>& OutVertices,
		TArray<FVector2D>& OutUVs,
		TArray<int32>& OutTriangles,
		TArray<FVector>& OutNormals,
		TArray<FProcMeshTangent>& OutTangents);
};

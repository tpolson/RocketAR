#include "ProceduralArcMesh.h"
#include "ProceduralMeshComponent.h"

void UProceduralArcMesh::GenerateArcMesh(
	float ArcAngleDegrees,
	float Radius,
	float Height,
	int32 Segments,
	TArray<FVector>& OutVertices,
	TArray<FVector2D>& OutUVs,
	TArray<int32>& OutTriangles,
	TArray<FVector>& OutNormals,
	TArray<FProcMeshTangent>& OutTangents)
{
	OutVertices.Empty();
	OutUVs.Empty();
	OutTriangles.Empty();
	OutNormals.Empty();
	OutTangents.Empty();

	Segments = FMath::Max(Segments, 3);
	const float HalfHeight = Height * 0.5f;
	const float ArcAngleRad = FMath::DegreesToRadians(ArcAngleDegrees);
	const float StartAngle = -ArcAngleRad * 0.5f;

	// Generate vertices: 2 rows (top and bottom) × (Segments + 1) columns
	const int32 VerticesPerRow = Segments + 1;

	for (int32 Row = 0; Row <= 1; ++Row)
	{
		const float Z = (Row == 0) ? -HalfHeight : HalfHeight;
		const float V = static_cast<float>(Row);

		for (int32 Col = 0; Col <= Segments; ++Col)
		{
			const float T = static_cast<float>(Col) / static_cast<float>(Segments);
			const float Angle = StartAngle + T * ArcAngleRad;

			// Position on the arc (looking from inside)
			const float X = Radius * FMath::Sin(Angle);
			const float Y = Radius * FMath::Cos(Angle);

			OutVertices.Add(FVector(X, Y, Z));

			// UV: U goes 0→1 along the arc, V goes 0→1 bottom to top
			OutUVs.Add(FVector2D(T, 1.0f - V));

			// Normal points inward (toward center)
			const FVector Normal = FVector(-FMath::Sin(Angle), -FMath::Cos(Angle), 0.0f);
			OutNormals.Add(Normal);

			// Tangent along the arc
			const FVector Tangent = FVector(FMath::Cos(Angle), -FMath::Sin(Angle), 0.0f);
			OutTangents.Add(FProcMeshTangent(Tangent, false));
		}
	}

	// Generate triangles (2 triangles per quad, CW winding for inside-facing)
	for (int32 Col = 0; Col < Segments; ++Col)
	{
		const int32 BL = Col;                    // Bottom-left
		const int32 BR = Col + 1;                // Bottom-right
		const int32 TL = Col + VerticesPerRow;   // Top-left
		const int32 TR = Col + 1 + VerticesPerRow; // Top-right

		// Triangle 1 (facing inward)
		OutTriangles.Add(BL);
		OutTriangles.Add(TL);
		OutTriangles.Add(BR);

		// Triangle 2
		OutTriangles.Add(BR);
		OutTriangles.Add(TL);
		OutTriangles.Add(TR);
	}
}

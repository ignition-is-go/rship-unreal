// Copyright Rocketship. All Rights Reserved.

#include "Rendering/SpatialTriangulation.h"
#include "RshipSpatialAudioRuntimeModule.h"

// ============================================================================
// FSpatialDelaunay2D
// ============================================================================

bool FSpatialDelaunay2D::Triangulate(const TArray<FVector2D>& InPoints)
{
	Points = InPoints;
	Triangles.Empty();
	bIsValid = false;

	if (Points.Num() < 3)
	{
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("Delaunay2D: Need at least 3 points"));
		return false;
	}

	// Find bounding box
	FVector2D MinPt(FLT_MAX, FLT_MAX);
	FVector2D MaxPt(-FLT_MAX, -FLT_MAX);
	for (const FVector2D& P : Points)
	{
		MinPt.X = FMath::Min(MinPt.X, P.X);
		MinPt.Y = FMath::Min(MinPt.Y, P.Y);
		MaxPt.X = FMath::Max(MaxPt.X, P.X);
		MaxPt.Y = FMath::Max(MaxPt.Y, P.Y);
	}

	// Create super-triangle that contains all points
	float DX = MaxPt.X - MinPt.X;
	float DY = MaxPt.Y - MinPt.Y;
	float DMax = FMath::Max(DX, DY) * 2.0f;

	FVector2D MidPt = (MinPt + MaxPt) * 0.5f;

	// Super-triangle vertices (added as temporary points)
	int32 SuperV0 = Points.Num();
	int32 SuperV1 = Points.Num() + 1;
	int32 SuperV2 = Points.Num() + 2;

	Points.Add(FVector2D(MidPt.X - DMax, MidPt.Y - DMax));
	Points.Add(FVector2D(MidPt.X, MidPt.Y + DMax));
	Points.Add(FVector2D(MidPt.X + DMax, MidPt.Y - DMax));

	// Start with super-triangle
	Triangles.Add(FSpatialTriangle2D(SuperV0, SuperV1, SuperV2));

	// Bowyer-Watson algorithm: insert points one by one
	int32 NumOriginalPoints = Points.Num() - 3;
	for (int32 i = 0; i < NumOriginalPoints; ++i)
	{
		const FVector2D& P = Points[i];

		// Find all triangles whose circumcircle contains the point
		TArray<FSpatialTriangle2D> BadTriangles;
		for (const FSpatialTriangle2D& Tri : Triangles)
		{
			if (IsPointInCircumcircle(P, Tri))
			{
				BadTriangles.Add(Tri);
			}
		}

		// Find the boundary of the polygonal hole
		TArray<FSpatialEdge2D> Polygon;
		for (const FSpatialTriangle2D& Tri : BadTriangles)
		{
			FSpatialEdge2D Edges[3] = {
				FSpatialEdge2D(Tri.V0, Tri.V1),
				FSpatialEdge2D(Tri.V1, Tri.V2),
				FSpatialEdge2D(Tri.V2, Tri.V0)
			};

			for (const FSpatialEdge2D& Edge : Edges)
			{
				// Check if this edge is shared with another bad triangle
				bool bShared = false;
				for (const FSpatialTriangle2D& Other : BadTriangles)
				{
					if (&Other == &Tri) continue;

					FSpatialEdge2D OtherEdges[3] = {
						FSpatialEdge2D(Other.V0, Other.V1),
						FSpatialEdge2D(Other.V1, Other.V2),
						FSpatialEdge2D(Other.V2, Other.V0)
					};

					for (const FSpatialEdge2D& OtherEdge : OtherEdges)
					{
						if (Edge == OtherEdge)
						{
							bShared = true;
							break;
						}
					}
					if (bShared) break;
				}

				if (!bShared)
				{
					Polygon.Add(Edge);
				}
			}
		}

		// Remove bad triangles
		for (const FSpatialTriangle2D& BadTri : BadTriangles)
		{
			Triangles.RemoveSingle(BadTri);
		}

		// Create new triangles from polygon edges to the new point
		for (const FSpatialEdge2D& Edge : Polygon)
		{
			Triangles.Add(FSpatialTriangle2D(Edge.V0, Edge.V1, i));
		}
	}

	// Remove triangles that contain super-triangle vertices
	Triangles.RemoveAll([SuperV0, SuperV1, SuperV2](const FSpatialTriangle2D& Tri) {
		return Tri.ContainsVertex(SuperV0) || Tri.ContainsVertex(SuperV1) || Tri.ContainsVertex(SuperV2);
	});

	// Remove super-triangle vertices from points array
	Points.SetNum(NumOriginalPoints);

	bIsValid = Triangles.Num() > 0;

	UE_LOG(LogRshipSpatialAudio, Verbose, TEXT("Delaunay2D: Created %d triangles from %d points"),
		Triangles.Num(), Points.Num());

	return bIsValid;
}

bool FSpatialDelaunay2D::TriangulateProjected(const TArray<FVector>& Points3D)
{
	TArray<FVector2D> Points2D;
	Points2D.Reserve(Points3D.Num());
	for (const FVector& P : Points3D)
	{
		Points2D.Add(FVector2D(P.X, P.Y));
	}
	return Triangulate(Points2D);
}

bool FSpatialDelaunay2D::FindContainingTriangle(const FVector2D& Point, int32& OutTriangleIndex) const
{
	OutTriangleIndex = -1;

	for (int32 i = 0; i < Triangles.Num(); ++i)
	{
		if (IsPointInTriangle(Point, Triangles[i]))
		{
			OutTriangleIndex = i;
			return true;
		}
	}

	return false;
}

bool FSpatialDelaunay2D::ComputeBarycentricCoords(
	const FVector2D& Point,
	int32 TriangleIndex,
	float& OutU, float& OutV, float& OutW) const
{
	if (TriangleIndex < 0 || TriangleIndex >= Triangles.Num())
	{
		OutU = OutV = OutW = 0.0f;
		return false;
	}

	const FSpatialTriangle2D& Tri = Triangles[TriangleIndex];
	const FVector2D& A = Points[Tri.V0];
	const FVector2D& B = Points[Tri.V1];
	const FVector2D& C = Points[Tri.V2];

	// Compute barycentric coordinates using cross products
	FVector2D V0 = C - A;
	FVector2D V1 = B - A;
	FVector2D V2 = Point - A;

	float Dot00 = FVector2D::DotProduct(V0, V0);
	float Dot01 = FVector2D::DotProduct(V0, V1);
	float Dot02 = FVector2D::DotProduct(V0, V2);
	float Dot11 = FVector2D::DotProduct(V1, V1);
	float Dot12 = FVector2D::DotProduct(V1, V2);

	float InvDenom = 1.0f / (Dot00 * Dot11 - Dot01 * Dot01);
	OutW = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;  // v (for V1/B)
	OutV = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;  // w (for V2/C)
	OutU = 1.0f - OutV - OutW;  // u (for V0/A)

	// Swap to match vertex order convention
	float TempU = OutU;
	OutU = TempU;  // V0
	// OutV already corresponds to V2 (C)
	// OutW already corresponds to V1 (B)
	float Temp = OutV;
	OutV = OutW;  // V1
	OutW = Temp;  // V2

	return OutU >= -0.001f && OutV >= -0.001f && OutW >= -0.001f;
}

bool FSpatialDelaunay2D::IsPointInCircumcircle(const FVector2D& P, const FSpatialTriangle2D& Tri) const
{
	const FVector2D& A = Points[Tri.V0];
	const FVector2D& B = Points[Tri.V1];
	const FVector2D& C = Points[Tri.V2];

	// Use determinant method for circumcircle test
	float Ax = A.X - P.X;
	float Ay = A.Y - P.Y;
	float Bx = B.X - P.X;
	float By = B.Y - P.Y;
	float Cx = C.X - P.X;
	float Cy = C.Y - P.Y;

	float Det = (Ax * Ax + Ay * Ay) * (Bx * Cy - Cx * By) -
	            (Bx * Bx + By * By) * (Ax * Cy - Cx * Ay) +
	            (Cx * Cx + Cy * Cy) * (Ax * By - Bx * Ay);

	// Positive determinant means P is inside circumcircle (assuming CCW orientation)
	return Det > 0.0f;
}

void FSpatialDelaunay2D::GetCircumcircle(const FSpatialTriangle2D& Tri, FVector2D& OutCenter, float& OutRadius) const
{
	const FVector2D& A = Points[Tri.V0];
	const FVector2D& B = Points[Tri.V1];
	const FVector2D& C = Points[Tri.V2];

	float D = 2.0f * (A.X * (B.Y - C.Y) + B.X * (C.Y - A.Y) + C.X * (A.Y - B.Y));

	if (FMath::Abs(D) < SMALL_NUMBER)
	{
		OutCenter = (A + B + C) / 3.0f;
		OutRadius = 0.0f;
		return;
	}

	float Ux = ((A.X * A.X + A.Y * A.Y) * (B.Y - C.Y) +
	            (B.X * B.X + B.Y * B.Y) * (C.Y - A.Y) +
	            (C.X * C.X + C.Y * C.Y) * (A.Y - B.Y)) / D;

	float Uy = ((A.X * A.X + A.Y * A.Y) * (C.X - B.X) +
	            (B.X * B.X + B.Y * B.Y) * (A.X - C.X) +
	            (C.X * C.X + C.Y * C.Y) * (B.X - A.X)) / D;

	OutCenter = FVector2D(Ux, Uy);
	OutRadius = FVector2D::Distance(OutCenter, A);
}

bool FSpatialDelaunay2D::IsPointInTriangle(const FVector2D& P, const FSpatialTriangle2D& Tri) const
{
	const FVector2D& A = Points[Tri.V0];
	const FVector2D& B = Points[Tri.V1];
	const FVector2D& C = Points[Tri.V2];

	// Compute barycentric coordinates
	FVector2D V0 = C - A;
	FVector2D V1 = B - A;
	FVector2D V2 = P - A;

	float Dot00 = FVector2D::DotProduct(V0, V0);
	float Dot01 = FVector2D::DotProduct(V0, V1);
	float Dot02 = FVector2D::DotProduct(V0, V2);
	float Dot11 = FVector2D::DotProduct(V1, V1);
	float Dot12 = FVector2D::DotProduct(V1, V2);

	float InvDenom = 1.0f / (Dot00 * Dot11 - Dot01 * Dot01);
	float U = (Dot11 * Dot02 - Dot01 * Dot12) * InvDenom;
	float V = (Dot00 * Dot12 - Dot01 * Dot02) * InvDenom;

	// Check if point is in triangle (with small tolerance for edge cases)
	return (U >= -0.001f) && (V >= -0.001f) && (U + V <= 1.001f);
}

int32 FSpatialDelaunay2D::FindContainingTriangle(const FVector2D& Point, FVector& OutBarycentricCoords) const
{
	OutBarycentricCoords = FVector::ZeroVector;

	int32 TriIndex = -1;
	if (FindContainingTriangle(Point, TriIndex))
	{
		float U, V, W;
		if (ComputeBarycentricCoords(Point, TriIndex, U, V, W))
		{
			OutBarycentricCoords = FVector(U, V, W);
			return TriIndex;
		}
	}

	return -1;
}

// ============================================================================
// FSpatialDelaunay3D
// ============================================================================

bool FSpatialDelaunay3D::Triangulate(const TArray<FVector>& InPoints)
{
	Points = InPoints;
	Tetrahedra.Empty();
	bIsValid = false;

	if (Points.Num() < 4)
	{
		UE_LOG(LogRshipSpatialAudio, Warning, TEXT("Delaunay3D: Need at least 4 points"));
		return false;
	}

	// Find bounding box
	FVector MinPt(FLT_MAX);
	FVector MaxPt(-FLT_MAX);
	for (const FVector& P : Points)
	{
		MinPt.X = FMath::Min(MinPt.X, P.X);
		MinPt.Y = FMath::Min(MinPt.Y, P.Y);
		MinPt.Z = FMath::Min(MinPt.Z, P.Z);
		MaxPt.X = FMath::Max(MaxPt.X, P.X);
		MaxPt.Y = FMath::Max(MaxPt.Y, P.Y);
		MaxPt.Z = FMath::Max(MaxPt.Z, P.Z);
	}

	// Create super-tetrahedron
	FVector Extent = MaxPt - MinPt;
	float DMax = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 3.0f;
	FVector MidPt = (MinPt + MaxPt) * 0.5f;

	int32 SuperV0 = Points.Num();
	int32 SuperV1 = Points.Num() + 1;
	int32 SuperV2 = Points.Num() + 2;
	int32 SuperV3 = Points.Num() + 3;

	Points.Add(FVector(MidPt.X - DMax, MidPt.Y - DMax, MidPt.Z - DMax));
	Points.Add(FVector(MidPt.X + DMax, MidPt.Y - DMax, MidPt.Z - DMax));
	Points.Add(FVector(MidPt.X, MidPt.Y + DMax, MidPt.Z - DMax));
	Points.Add(FVector(MidPt.X, MidPt.Y, MidPt.Z + DMax));

	Tetrahedra.Add(FSpatialTetrahedron(SuperV0, SuperV1, SuperV2, SuperV3));

	// Bowyer-Watson for 3D
	int32 NumOriginalPoints = Points.Num() - 4;
	for (int32 i = 0; i < NumOriginalPoints; ++i)
	{
		const FVector& P = Points[i];

		// Find bad tetrahedra
		TArray<FSpatialTetrahedron> BadTetra;
		for (const FSpatialTetrahedron& Tet : Tetrahedra)
		{
			if (IsPointInCircumsphere(P, Tet))
			{
				BadTetra.Add(Tet);
			}
		}

		// Find boundary triangular faces
		TArray<FSpatialTriangle2D> BoundaryFaces;  // Reusing Triangle2D as face storage

		for (const FSpatialTetrahedron& Tet : BadTetra)
		{
			// 4 faces per tetrahedron
			FSpatialTriangle2D Faces[4] = {
				FSpatialTriangle2D(Tet.V0, Tet.V1, Tet.V2),
				FSpatialTriangle2D(Tet.V0, Tet.V1, Tet.V3),
				FSpatialTriangle2D(Tet.V0, Tet.V2, Tet.V3),
				FSpatialTriangle2D(Tet.V1, Tet.V2, Tet.V3)
			};

			for (const FSpatialTriangle2D& Face : Faces)
			{
				bool bShared = false;
				for (const FSpatialTetrahedron& Other : BadTetra)
				{
					if (&Other == &Tet) continue;

					FSpatialTriangle2D OtherFaces[4] = {
						FSpatialTriangle2D(Other.V0, Other.V1, Other.V2),
						FSpatialTriangle2D(Other.V0, Other.V1, Other.V3),
						FSpatialTriangle2D(Other.V0, Other.V2, Other.V3),
						FSpatialTriangle2D(Other.V1, Other.V2, Other.V3)
					};

					for (const FSpatialTriangle2D& OtherFace : OtherFaces)
					{
						if (Face == OtherFace)
						{
							bShared = true;
							break;
						}
					}
					if (bShared) break;
				}

				if (!bShared)
				{
					BoundaryFaces.Add(Face);
				}
			}
		}

		// Remove bad tetrahedra
		for (const FSpatialTetrahedron& Bad : BadTetra)
		{
			Tetrahedra.RemoveAll([&Bad](const FSpatialTetrahedron& T) {
				return T.V0 == Bad.V0 && T.V1 == Bad.V1 && T.V2 == Bad.V2 && T.V3 == Bad.V3;
			});
		}

		// Create new tetrahedra
		for (const FSpatialTriangle2D& Face : BoundaryFaces)
		{
			Tetrahedra.Add(FSpatialTetrahedron(Face.V0, Face.V1, Face.V2, i));
		}
	}

	// Remove tetrahedra with super-tetrahedron vertices
	Tetrahedra.RemoveAll([SuperV0, SuperV1, SuperV2, SuperV3](const FSpatialTetrahedron& Tet) {
		return Tet.ContainsVertex(SuperV0) || Tet.ContainsVertex(SuperV1) ||
		       Tet.ContainsVertex(SuperV2) || Tet.ContainsVertex(SuperV3);
	});

	Points.SetNum(NumOriginalPoints);

	bIsValid = Tetrahedra.Num() > 0;

	UE_LOG(LogRshipSpatialAudio, Verbose, TEXT("Delaunay3D: Created %d tetrahedra from %d points"),
		Tetrahedra.Num(), Points.Num());

	return bIsValid;
}

bool FSpatialDelaunay3D::FindContainingTetrahedron(const FVector& Point, int32& OutTetraIndex) const
{
	OutTetraIndex = -1;

	for (int32 i = 0; i < Tetrahedra.Num(); ++i)
	{
		if (IsPointInTetrahedron(Point, Tetrahedra[i]))
		{
			OutTetraIndex = i;
			return true;
		}
	}

	return false;
}

bool FSpatialDelaunay3D::ComputeBarycentricCoords(
	const FVector& Point,
	int32 TetraIndex,
	float OutCoords[4]) const
{
	if (TetraIndex < 0 || TetraIndex >= Tetrahedra.Num())
	{
		OutCoords[0] = OutCoords[1] = OutCoords[2] = OutCoords[3] = 0.0f;
		return false;
	}

	const FSpatialTetrahedron& Tet = Tetrahedra[TetraIndex];
	const FVector& A = Points[Tet.V0];
	const FVector& B = Points[Tet.V1];
	const FVector& C = Points[Tet.V2];
	const FVector& D = Points[Tet.V3];

	// Compute barycentric coordinates using signed volumes
	float VolTotal = SignedVolume(A, B, C, D);

	if (FMath::Abs(VolTotal) < SMALL_NUMBER)
	{
		OutCoords[0] = OutCoords[1] = OutCoords[2] = OutCoords[3] = 0.25f;
		return false;
	}

	OutCoords[0] = SignedVolume(Point, B, C, D) / VolTotal;
	OutCoords[1] = SignedVolume(A, Point, C, D) / VolTotal;
	OutCoords[2] = SignedVolume(A, B, Point, D) / VolTotal;
	OutCoords[3] = SignedVolume(A, B, C, Point) / VolTotal;

	return OutCoords[0] >= -0.001f && OutCoords[1] >= -0.001f &&
	       OutCoords[2] >= -0.001f && OutCoords[3] >= -0.001f;
}

bool FSpatialDelaunay3D::IsPointInCircumsphere(const FVector& P, const FSpatialTetrahedron& Tet) const
{
	const FVector& A = Points[Tet.V0];
	const FVector& B = Points[Tet.V1];
	const FVector& C = Points[Tet.V2];
	const FVector& D = Points[Tet.V3];

	// 5x5 determinant for circumsphere test
	// This is the 3D equivalent of the circumcircle test

	FVector PA = A - P;
	FVector PB = B - P;
	FVector PC = C - P;
	FVector PD = D - P;

	float A2 = PA.SizeSquared();
	float B2 = PB.SizeSquared();
	float C2 = PC.SizeSquared();
	float D2 = PD.SizeSquared();

	// Compute 4x4 determinant (avoiding full 5x5)
	float Det = A2 * SignedVolume(PB, PC, PD, FVector::ZeroVector) -
	            B2 * SignedVolume(PA, PC, PD, FVector::ZeroVector) +
	            C2 * SignedVolume(PA, PB, PD, FVector::ZeroVector) -
	            D2 * SignedVolume(PA, PB, PC, FVector::ZeroVector);

	// Sign depends on tetrahedron orientation
	float Orient = SignedVolume(A, B, C, D);
	return (Orient > 0) ? (Det > 0) : (Det < 0);
}

bool FSpatialDelaunay3D::IsPointInTetrahedron(const FVector& P, const FSpatialTetrahedron& Tet) const
{
	const FVector& A = Points[Tet.V0];
	const FVector& B = Points[Tet.V1];
	const FVector& C = Points[Tet.V2];
	const FVector& D = Points[Tet.V3];

	// Point is inside if it's on the same side of each face as the opposite vertex
	float D0 = SignedVolume(A, B, C, D);
	float D1 = SignedVolume(P, B, C, D);
	float D2 = SignedVolume(A, P, C, D);
	float D3 = SignedVolume(A, B, P, D);
	float D4 = SignedVolume(A, B, C, P);

	bool HasPos = (D0 > 0) || (D1 > 0) || (D2 > 0) || (D3 > 0) || (D4 > 0);
	bool HasNeg = (D0 < 0) || (D1 < 0) || (D2 < 0) || (D3 < 0) || (D4 < 0);

	// All same sign or zero means inside (with tolerance)
	return !(HasPos && HasNeg);
}

float FSpatialDelaunay3D::SignedVolume(const FVector& A, const FVector& B, const FVector& C, const FVector& D) const
{
	return FVector::DotProduct(A - D, FVector::CrossProduct(B - D, C - D)) / 6.0f;
}

int32 FSpatialDelaunay3D::FindContainingTetrahedron(const FVector& Point, FVector4& OutBarycentricCoords) const
{
	OutBarycentricCoords = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	int32 TetIndex = -1;
	if (FindContainingTetrahedron(Point, TetIndex))
	{
		float Coords[4];
		if (ComputeBarycentricCoords(Point, TetIndex, Coords))
		{
			OutBarycentricCoords = FVector4(Coords[0], Coords[1], Coords[2], Coords[3]);
			return TetIndex;
		}
	}

	return -1;
}

// Copyright Rocketship. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Triangle in a 2D Delaunay triangulation.
 * Vertices are indices into the original point array.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialTriangle2D
{
	union
	{
		struct { int32 V0, V1, V2; };  // Vertex indices (counter-clockwise)
		int32 Indices[3];
	};

	FSpatialTriangle2D() : V0(-1), V1(-1), V2(-1) {}
	FSpatialTriangle2D(int32 InV0, int32 InV1, int32 InV2) : V0(InV0), V1(InV1), V2(InV2) {}

	bool ContainsVertex(int32 V) const { return V0 == V || V1 == V || V2 == V; }
	bool IsValid() const { return V0 >= 0 && V1 >= 0 && V2 >= 0; }

	bool operator==(const FSpatialTriangle2D& Other) const
	{
		// Order-independent comparison
		TArray<int32> A = { V0, V1, V2 };
		TArray<int32> B = { Other.V0, Other.V1, Other.V2 };
		A.Sort();
		B.Sort();
		return A[0] == B[0] && A[1] == B[1] && A[2] == B[2];
	}
};

/**
 * Edge in the triangulation.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialEdge2D
{
	int32 V0, V1;

	FSpatialEdge2D() : V0(-1), V1(-1) {}
	FSpatialEdge2D(int32 InV0, int32 InV1) : V0(FMath::Min(InV0, InV1)), V1(FMath::Max(InV0, InV1)) {}

	bool operator==(const FSpatialEdge2D& Other) const
	{
		return V0 == Other.V0 && V1 == Other.V1;
	}

	friend uint32 GetTypeHash(const FSpatialEdge2D& Edge)
	{
		return HashCombine(GetTypeHash(Edge.V0), GetTypeHash(Edge.V1));
	}
};

/**
 * Tetrahedron in a 3D Delaunay triangulation.
 */
struct RSHIPSPATIALAUDIORUNTIME_API FSpatialTetrahedron
{
	union
	{
		struct { int32 V0, V1, V2, V3; };  // Vertex indices
		int32 Indices[4];
	};

	FSpatialTetrahedron() : V0(-1), V1(-1), V2(-1), V3(-1) {}
	FSpatialTetrahedron(int32 InV0, int32 InV1, int32 InV2, int32 InV3)
		: V0(InV0), V1(InV1), V2(InV2), V3(InV3) {}

	bool ContainsVertex(int32 V) const { return V0 == V || V1 == V || V2 == V || V3 == V; }
	bool IsValid() const { return V0 >= 0 && V1 >= 0 && V2 >= 0 && V3 >= 0; }
};

/**
 * 2D Delaunay triangulation using Bowyer-Watson algorithm.
 *
 * Used for VBAP in 2D speaker arrays (horizontal plane).
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialDelaunay2D
{
public:
	FSpatialDelaunay2D() = default;

	/**
	 * Compute Delaunay triangulation for a set of 2D points.
	 * Points are projected onto the XY plane (Z is ignored).
	 *
	 * @param Points Input points (only X, Y used).
	 * @return True if triangulation succeeded.
	 */
	bool Triangulate(const TArray<FVector2D>& Points);

	/**
	 * Triangulate 3D points projected onto the horizontal plane (XY).
	 */
	bool TriangulateProjected(const TArray<FVector>& Points3D);

	/**
	 * Find the triangle containing a given point.
	 *
	 * @param Point The query point.
	 * @param OutTriangleIndex Output: index of containing triangle (-1 if not found).
	 * @return True if point is inside a triangle.
	 */
	bool FindContainingTriangle(const FVector2D& Point, int32& OutTriangleIndex) const;

	/**
	 * Compute barycentric coordinates for a point within a triangle.
	 *
	 * @param Point The query point.
	 * @param TriangleIndex Index of the triangle.
	 * @param OutU, OutV, OutW Output barycentric coordinates (sum to 1).
	 * @return True if coordinates are valid (point inside triangle).
	 */
	bool ComputeBarycentricCoords(
		const FVector2D& Point,
		int32 TriangleIndex,
		float& OutU, float& OutV, float& OutW) const;

	/**
	 * Find containing triangle and compute barycentric coordinates in one call.
	 * Convenience method for VBAP gain computation.
	 *
	 * @param Point The query point (unit vector on XY plane).
	 * @param OutBarycentricCoords Output barycentric coords as FVector (X=U, Y=V, Z=W).
	 * @return Triangle index, or -1 if point outside all triangles.
	 */
	int32 FindContainingTriangle(const FVector2D& Point, FVector& OutBarycentricCoords) const;

	/** Public access to triangles for iteration */
	TArray<FSpatialTriangle2D> Triangles;

	/** Get the triangulation results */
	const TArray<FSpatialTriangle2D>& GetTriangles() const { return Triangles; }
	const TArray<FVector2D>& GetPoints() const { return Points; }
	int32 GetTriangleCount() const { return Triangles.Num(); }

	/** Check if triangulation is valid */
	bool IsValid() const { return bIsValid && Triangles.Num() > 0; }

private:
	TArray<FVector2D> Points;
	bool bIsValid = false;

	// Circumcircle test
	bool IsPointInCircumcircle(const FVector2D& P, const FSpatialTriangle2D& Tri) const;

	// Get circumcircle center and radius
	void GetCircumcircle(const FSpatialTriangle2D& Tri, FVector2D& OutCenter, float& OutRadius) const;

	// Check if point is inside triangle
	bool IsPointInTriangle(const FVector2D& P, const FSpatialTriangle2D& Tri) const;
};

/**
 * 3D Delaunay tetrahedralization using Bowyer-Watson algorithm.
 *
 * Used for VBAP in full 3D speaker configurations.
 */
class RSHIPSPATIALAUDIORUNTIME_API FSpatialDelaunay3D
{
public:
	FSpatialDelaunay3D() = default;

	/**
	 * Compute Delaunay tetrahedralization for a set of 3D points.
	 *
	 * @param Points Input points.
	 * @return True if triangulation succeeded.
	 */
	bool Triangulate(const TArray<FVector>& Points);

	/**
	 * Find the tetrahedron containing a given point.
	 *
	 * @param Point The query point.
	 * @param OutTetraIndex Output: index of containing tetrahedron (-1 if not found).
	 * @return True if point is inside a tetrahedron.
	 */
	bool FindContainingTetrahedron(const FVector& Point, int32& OutTetraIndex) const;

	/**
	 * Compute barycentric coordinates for a point within a tetrahedron.
	 *
	 * @param Point The query point.
	 * @param TetraIndex Index of the tetrahedron.
	 * @param OutCoords Output barycentric coordinates [4] (sum to 1).
	 * @return True if coordinates are valid (point inside tetrahedron).
	 */
	bool ComputeBarycentricCoords(
		const FVector& Point,
		int32 TetraIndex,
		float OutCoords[4]) const;

	/**
	 * Find containing tetrahedron and compute barycentric coordinates in one call.
	 * Convenience method for VBAP gain computation.
	 *
	 * @param Point The query point (unit direction vector).
	 * @param OutBarycentricCoords Output barycentric coords as FVector4 (X,Y,Z,W).
	 * @return Tetrahedron index, or -1 if point outside all tetrahedra.
	 */
	int32 FindContainingTetrahedron(const FVector& Point, FVector4& OutBarycentricCoords) const;

	/** Public access to tetrahedra for iteration */
	TArray<FSpatialTetrahedron> Tetrahedra;

	/** Get the triangulation results */
	const TArray<FSpatialTetrahedron>& GetTetrahedra() const { return Tetrahedra; }
	const TArray<FVector>& GetPoints() const { return Points; }
	int32 GetTetrahedronCount() const { return Tetrahedra.Num(); }

	/** Check if triangulation is valid */
	bool IsValid() const { return bIsValid && Tetrahedra.Num() > 0; }

private:
	TArray<FVector> Points;
	bool bIsValid = false;

	// Circumsphere test
	bool IsPointInCircumsphere(const FVector& P, const FSpatialTetrahedron& Tet) const;

	// Check if point is inside tetrahedron
	bool IsPointInTetrahedron(const FVector& P, const FSpatialTetrahedron& Tet) const;

	// Compute signed volume (for orientation tests)
	float SignedVolume(const FVector& A, const FVector& B, const FVector& C, const FVector& D) const;
};

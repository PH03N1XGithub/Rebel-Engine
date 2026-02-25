#pragma once
#include <vector>

namespace PhysicsDebug
{
	void DrawLine(const Vector3& a, const Vector3& b, const Vector3& color);
	void DrawBox(const Vector3& min, const Vector3& max, const Vector3& color);
	void Clear(); // called once per frame

	struct DebugLineVertex
	{
		Vector3 Pos;
		Vector3 Color;
	};

	// RenderModule reads these each frame
	const std::vector<DebugLineVertex>& GetLineVertices();
}

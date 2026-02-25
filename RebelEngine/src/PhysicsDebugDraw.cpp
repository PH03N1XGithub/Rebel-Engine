#include "EnginePch.h"
#include "PhysicsDebugDraw.h"

#include <vector>

namespace PhysicsDebug
{
	
	static std::vector<DebugLineVertex> g_LineVerts;

	static inline void PushLine(const Vector3& a, const Vector3& b, const Vector3& c)
	{
		g_LineVerts.push_back({ a, c });
		g_LineVerts.push_back({ b, c });
	}

	void DrawLine(const Vector3& a, const Vector3& b, const Vector3& color)
	{
		PushLine(a, b, color);
	}

	// Axis-aligned box (min/max) convenience
	void DrawBox(const Vector3& min, const Vector3& max, const Vector3& color)
	{
		const Vector3 p000(min.x, min.y, min.z);
		const Vector3 p001(min.x, min.y, max.z);
		const Vector3 p010(min.x, max.y, min.z);
		const Vector3 p011(min.x, max.y, max.z);

		const Vector3 p100(max.x, min.y, min.z);
		const Vector3 p101(max.x, min.y, max.z);
		const Vector3 p110(max.x, max.y, min.z);
		const Vector3 p111(max.x, max.y, max.z);

		// bottom
		PushLine(p000, p100, color);
		PushLine(p100, p110, color);
		PushLine(p110, p010, color);
		PushLine(p010, p000, color);

		// top
		PushLine(p001, p101, color);
		PushLine(p101, p111, color);
		PushLine(p111, p011, color);
		PushLine(p011, p001, color);

		// verticals
		PushLine(p000, p001, color);
		PushLine(p100, p101, color);
		PushLine(p110, p111, color);
		PushLine(p010, p011, color);
	}

	void Clear()
	{
		g_LineVerts.clear();
	}

	// Internal accessor (used by RenderModule)
	const std::vector<DebugLineVertex>& GetLineVertices()
	{
		return g_LineVerts;
	}
}

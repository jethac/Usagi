#include "Engine/Common/Common.h"
#include "Engine/Maths/AABB.h"
#include "Engine/Maths/Matrix4x4.h"
#include "Engine/Maths/Quaternionf.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace usg
{
	MemType g_uDefaultMemType = MEMTYPE_STANDARD;

	namespace mem
	{
		void* Alloc(MemType, MemAllocType, memsize uSize, uint32 uAlign, bool)
		{
			UNUSED_VAR(uAlign);
			return new char[uSize];
		}

		void Free(MemType, void* pData, bool)
		{
			delete[] (char*)pData;
		}

		void Free(void* pData)
		{
			delete[] (char*)pData;
		}
	}
}

namespace eastl
{
	void AssertionFailure(const char* pExpression)
	{
		printf("EASTL ASSERT FAILED: %s\n", pExpression);
		abort();
	}
}

namespace
{
	bool Expect(bool condition, const char* szMessage)
	{
		if (!condition)
		{
			printf("FAILED: %s\n", szMessage);
			return false;
		}
		return true;
	}

	bool NearlyEqual(float a, float b, float epsilon = 1e-4f)
	{
		return fabsf(a - b) <= epsilon;
	}

	bool ExpectVector(const usg::Vector3f& value, const usg::Vector3f& expected, const char* szMessage)
	{
		const bool ok = NearlyEqual(value.x, expected.x) && NearlyEqual(value.y, expected.y) && NearlyEqual(value.z, expected.z);
		if (!ok)
		{
			printf("FAILED: %s got=(%.6f, %.6f, %.6f) expected=(%.6f, %.6f, %.6f)\n",
				szMessage, value.x, value.y, value.z, expected.x, expected.y, expected.z);
		}
		return ok;
	}

	bool TestMatrixTranslationUsesVectorW()
	{
		usg::Matrix4x4 transform = usg::Matrix4x4::Identity();
		transform.SetTranslation(2.0f, -3.0f, 5.0f);

		bool ok = true;
		ok &= ExpectVector(transform.TransformVec3(usg::Vector3f(1.0f, 2.0f, 3.0f), 1.0f),
			usg::Vector3f(3.0f, -1.0f, 8.0f),
			"position transform applies translation");
		ok &= ExpectVector(transform.TransformVec3(usg::Vector3f(1.0f, 2.0f, 3.0f), 0.0f),
			usg::Vector3f(1.0f, 2.0f, 3.0f),
			"direction transform ignores translation");
		return ok;
	}

	bool TestQuaternionAxisRotation()
	{
		const usg::Quaternionf quarterTurn(usg::Vector3f::Z_AXIS, usg::Math::pi_over_2);
		const usg::Vector3f rotated = usg::Vector3f::X_AXIS * quarterTurn;

		return ExpectVector(rotated, usg::Vector3f(0.0f, 1.0f, 0.0f),
			"positive Z quarter-turn rotates X onto Y");
	}

	bool TestAABBContainmentAndLerp()
	{
		usg::AABB box;
		box.SetCentreRadii(usg::Vector3f(0.0f, 0.0f, 0.0f), usg::Vector3f(1.0f, 2.0f, 3.0f));

		bool ok = true;
		ok &= Expect(box.InBox(usg::Vector3f(1.0f, -2.0f, 3.0f)), "AABB includes boundary points");
		ok &= Expect(!box.InBox(usg::Vector3f(1.1f, 0.0f, 0.0f)), "AABB rejects points outside one axis");

		usg::AABB start(usg::Vector3f(-1.0f, -1.0f, -1.0f), usg::Vector3f(1.0f, 1.0f, 1.0f));
		usg::AABB end(usg::Vector3f(1.0f, 3.0f, 5.0f), usg::Vector3f(5.0f, 7.0f, 9.0f));
		usg::AABB midpoint;
		usg::Lerp(start, end, midpoint, 0.5f);

		usg::Vector3f centre;
		usg::Vector3f radii;
		midpoint.GetCentreRadii(centre, radii);
		ok &= ExpectVector(centre, usg::Vector3f(1.5f, 2.5f, 3.5f), "AABB lerp interpolates centre");
		ok &= ExpectVector(radii, usg::Vector3f(1.5f, 1.5f, 1.5f), "AABB lerp interpolates radii");
		return ok;
	}
}

int main()
{
	bool ok = true;
	ok &= TestMatrixTranslationUsesVectorW();
	ok &= TestQuaternionAxisRotation();
	ok &= TestAABBContainmentAndLerp();

	if (!ok)
	{
		return 1;
	}

	printf("Maths tests passed\n");
	return 0;
}

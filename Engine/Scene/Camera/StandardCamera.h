/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: A world view
*****************************************************************************/
#ifndef _USG_STANDARD_CAMERA_H_
#define _USG_STANDARD_CAMERA_H_

#include "Engine/Scene/Camera/Camera.h"

namespace usg
{


	class StandardCamera : public Camera
	{
	public:
		StandardCamera(void);
		~StandardCamera(void);

		virtual void SetModelMatrix(const Matrix4x4& mModel) override;
		// Perspective projection; fFov is the vertical field of view in degrees.
		void SetUp(const Matrix4x4& viewMat, float32 fAspect, float32 fFov, float32 fNear, float32 fFar);
		// Orthographic projection centred on the view axis; fHalfHeight is half of
		// the visible height in world units (the half width is fHalfHeight * fAspect).
		void SetUpOrthographic(const Matrix4x4& viewMat, float32 fAspect, float32 fHalfHeight, float32 fNear, float32 fFar);
		void SetUp(const Matrix4x4& viewMat, const Matrix4x4& projMat);

		bool IsOrthographic() const { return m_bOrthographic; }
		// Half of the visible height in world units; only meaningful when orthographic.
		float32 GetOrthographicSize() const { return m_fOrthographicSize; }

		virtual const Matrix4x4& GetProjection(ViewType eType = VIEW_CENTRAL) const;
		virtual const Matrix4x4& GetViewMatrix(ViewType eType = VIEW_CENTRAL) const;
		virtual float GetNear(ViewType eType = VIEW_CENTRAL) const { return m_fNear; }
		virtual float GetFar(ViewType eType = VIEW_CENTRAL) const { return m_fFar; }

		virtual bool Is3D() const { return false; }

	private:
		void UpdateInternal();

		Matrix4x4	m_mView;
		Matrix4x4	m_mProjection;
		float 		m_fNear;
		float 		m_fFar;
		float		m_fOrthographicSize;
		bool		m_bOrthographic;
	};

}

#endif

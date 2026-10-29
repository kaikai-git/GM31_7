#pragma once

#include	"commontypes.h"
#include	"renderer.h"
#include	"CPolar3D.h"
#include	"../application.h"

class Camera {
protected:
	Vector3	m_position = Vector3(0.0f, 0.0f, 0.0f);	// カメラ位置

	Vector3		m_lookat{};				// 注視点
	Vector3		m_up = { 0,1,0 };		// アップベクトル			
	Matrix4x4	m_viewmtx{};			// ビュー変換行列
	Matrix4x4   m_projmtx{};			// プロジェクション行列

public:
	virtual ~Camera(){}
	Camera() = default;

	Camera(Vector3 pos, Vector3 lookat,Vector3 up)
		:m_position(pos),m_lookat(lookat),m_up(up) {}

	void Init();
	void Dispose();
	void Update();
	void Draw();
	void SetPosition(const Vector3& position) { m_position = position; }
	void SetLookat(const Vector3& position) { m_lookat = position; }
	void SetUP(const Vector3& up) { m_up = up; }

	void Draw2D() {
		Renderer::SetWorldViewProjection2D();
	}


	Matrix4x4 GetViewMatrix() const { return m_viewmtx; }
	Matrix4x4 GetProjMatrix() const { return m_projmtx; }

	Vector3 GetPosition() const{
		return m_position;
	}
	Vector3 GetLookat() const{
		return m_lookat;
	}
	Vector3 GetUP() const {
		return m_up;
	}
};

class FreeCamera : public Camera {
public:
	FreeCamera(Vector3 pos, Vector3 lookat, Vector3 up)
		: Camera(pos, lookat, up) {}

	void SetAzimuth(float azimuth) {
		m_azimuth = azimuth;
	}

	void SetElevation(float elevation) {
		m_elevation = elevation;
	}

	void SetRadius(float radius) {
		m_radius = radius;
	}

	void Update() {
		// 仰角と方位角からカメラの位置を計算
		CPolor3D polar(m_radius, m_elevation, m_azimuth);

		m_position = polar.ToCartesian();

		CPolor3D polarup(m_radius, (
			m_elevation + 90.0f * PI / 180.0f),
			m_azimuth);

		m_up = polarup.ToCartesian();
	}

private:
	float m_azimuth = 0.0f;			// 水平角度
	float m_elevation = -(PI/180.0f)*80.0f;	// 垂直角度
	float m_radius = 100.0f;		// 半径
};
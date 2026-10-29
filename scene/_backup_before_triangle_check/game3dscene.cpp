#include "../gameobject/field.h"
#include "../gameobject/player.h"
#include "Game3DScene.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../system/C3DShape.h"
#include "../system/camera.h"
#include "../system/collision.h"
#include "../system/commontypes.h"
#include "../system/CShader.h"
#include "../system/CStaticMesh.h"
#include "../system/CStaticMeshRenderer.h"
#include "../system/DebugUI.h"
#include "../system/DirectWrite.h"
#include "../system/meshmanager.h"
#include "../system/renderer.h"

namespace {

	struct Load3DInfo {
		std::string meshid{};
		std::string filename;
		std::string texdirectoryname;
		Load3DInfo(std::string id, std::string p1, std::string p2) {
			meshid = id;
			filename = p1;
			texdirectoryname = p2;
		}
	};

	std::string getfilename(std::string_view filestring) {
		auto u8name = std::filesystem::path(filestring).filename().u8string();
		return { reinterpret_cast<const char*>(u8name.data()), u8name.size() };
	}

	std::array<Load3DInfo, 3> g_loadmodel =
	{
			Load3DInfo(
				"car000.x",
				"assets/model/car000.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"car001.x",
				"assets/model/car001.x",			// モデル名
				"assets/model/"),					// テクスチャのパス

			Load3DInfo(
				"car002.x",
				"assets/model/car002.x",			// モデル名
				"assets/model/"),					// テクスチャのパス
	};

	void OBBDraw(GM31::GE::Collision::BoundingBoxOBB obb,Color col)
	{
		// 中心軸を正規化しておく（軸に長さが混ざっていても正しく描画するため）
		Vector3 axisx = obb.axisX;
		Vector3 axisy = obb.axisY;
		Vector3 axisz = obb.axisZ;

		axisx.Normalize();
		axisy.Normalize();
		axisz.Normalize();

		Matrix4x4 mtx = Matrix4x4::Identity;

		// 各行に「軸 × 辺の長さ」を入れる（拡大と回転を同時に行う行列になる）
		mtx._11 = axisx.x * obb.lengthx;
		mtx._12 = axisx.y * obb.lengthx;
		mtx._13 = axisx.z * obb.lengthx;

		mtx._21 = axisy.x * obb.lengthy;
		mtx._22 = axisy.y * obb.lengthy;
		mtx._23 = axisy.z * obb.lengthy;

		mtx._31 = axisz.x * obb.lengthz;
		mtx._32 = axisz.y * obb.lengthz;
		mtx._33 = axisz.z * obb.lengthz;

		// 平行移動（OBBの中心座標＝ワールド座標）
		mtx._41 = obb.worldcenter.x;
		mtx._42 = obb.worldcenter.y;
		mtx._43 = obb.worldcenter.z;

		BoxDrawerDraw(mtx, col);
	}

	void BSDraw(GM31::GE::Collision::BoundingSphere bs, Color col)
	{
		Matrix4x4 mtx = Matrix4x4::Identity;

		mtx._11 *= bs.radius;
		mtx._12 *= bs.radius;
		mtx._13 *= bs.radius;

		mtx._21 *= bs.radius;
		mtx._22 *= bs.radius;
		mtx._23 *= bs.radius;

		mtx._31 *= bs.radius;
		mtx._32 *= bs.radius;
		mtx._33 *= bs.radius;

		// 平行移動（OBBの中心座標＝ワールド座標）
		mtx._41 = bs.center.x;
		mtx._42 = bs.center.y;
		mtx._43 = bs.center.z;

		SphereDrawerDraw(mtx, col);
	}

	// 指定位置に球を描画
	void PointDraw(const Vector3& pos, float radius, Color col)
	{
		GM31::GE::Collision::BoundingSphere bs{};
		bs.center = pos;
		bs.radius = radius;
		BSDraw(bs, col);
	}

	// 2点間に線分を描画
	void LineDraw(const Vector3& start, const Vector3& end, Color col, float width = 2.0f)
	{
		Vector3 dir = end - start;
		float length = dir.Length();
		if (length <= 0.0f) return;

		SetLineWidth(width);
		LineDrawerDraw(length, start, dir, col);	// 内部で dir は正規化される
	}

	// 平面を描画（法線方向に薄い箱で表示する）
	void PlaneDraw(const PLANEINFO& info, float size, Color col)
	{
		Vector3 normal(info.plane.a, info.plane.b, info.plane.c);

		// 平面上の2軸を作る（u: p0→p1方向、v: u×法線）
		Vector3 u = info.p1 - info.p0;
		u.Normalize();
		Vector3 v = u.Cross(normal);
		v.Normalize();

		// 3頂点の重心を平面の中心にする
		Vector3 center = (info.p0 + info.p1 + info.p2) / 3.0f;

		const float thickness = 0.1f;

		Matrix4x4 mtx = Matrix4x4::Identity;

		// X軸 → u 方向（大きさ size）
		mtx._11 = u.x * size;
		mtx._12 = u.y * size;
		mtx._13 = u.z * size;

		// Y軸 → 法線方向（厚み）
		mtx._21 = normal.x * thickness;
		mtx._22 = normal.y * thickness;
		mtx._23 = normal.z * thickness;

		// Z軸 → v 方向（大きさ size）
		mtx._31 = v.x * size;
		mtx._32 = v.y * size;
		mtx._33 = v.z * size;

		// 平行移動
		mtx._41 = center.x;
		mtx._42 = center.y;
		mtx._43 = center.z;

		BoxDrawerDraw(mtx, col);
	}
}

/**
 * @brief コンストラクタ
 */
Game3DScene::Game3DScene()
{
}

/**
 * @brief シーンの更新処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */
void Game3DScene::update(uint64_t deltatime)
{
	// 全オブジェクトを処理
	for (int layer = 0; layer < LAYERMAX; ++layer)
	{
		auto& objects = m_gameobjects[layer];

		for (auto& obj : objects)
		{
			if (obj && !obj->isDestroyed())
			{
				obj->update(deltatime);
			}
		}
	}

	// 削除
	for (int layer = 0; layer < LAYERMAX; ++layer)
	{
		auto& objects = m_gameobjects[layer];

		std::erase_if(
			objects,
			[](const std::unique_ptr<gameobject>& obj)
			{
				return obj == nullptr || obj->isDestroyed();
			});
	}

	// 追加
	for (int layer = 0; layer < LAYERMAX; ++layer)
	{
		auto& objects = m_gameobjects[layer];
		auto& pending = m_pendingAddObjects[layer];

		for (auto& obj : pending)
		{
			objects.push_back(std::move(obj));
		}

		pending.clear();
	}

}

/**
 * @brief 描画処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */

void Game3DScene::draw(uint64_t deltatime)
{
	m_camera->Draw();

	// 3本のworld軸を描画
	for (int cnt = 0; cnt < m_segments.size(); cnt++)
	{
		Matrix4x4 worldmtx = Matrix4x4::Identity;
		m_segments[cnt]->SetWidth(1.0f);
		m_segments[cnt]->Draw(worldmtx, Color(0, 0, 0, 1));
	}

	// ---------------------------------------------
	// 平面の3頂点を回転させる（3頂点の重心を中心にXYZ回転）
	// ---------------------------------------------
	Vector3 localcenter = (m_planevertices[0] + m_planevertices[1] + m_planevertices[2]) / 3.0f;

	Matrix4x4 rotmtx = Matrix4x4::CreateFromYawPitchRoll(
		m_planerotation.y * PI / 180.0f,		// Y軸回転（度→ラジアン）
		m_planerotation.x * PI / 180.0f,		// X軸回転
		m_planerotation.z * PI / 180.0f);		// Z軸回転

	for (int cnt = 0; cnt < 3; cnt++)
	{
		// 重心を原点に移動 → 回転 → 重心の位置に戻す
		m_rotatedvertices[cnt] = Vector3::Transform(m_planevertices[cnt] - localcenter, rotmtx) + localcenter;
	}

	// 3頂点が一直線上にないかチェック（外積の長さが0なら平面が作れない）
	Vector3 p0p1 = m_rotatedvertices[1] - m_rotatedvertices[0];
	Vector3 p1p2 = m_rotatedvertices[2] - m_rotatedvertices[1];
	m_planevalid = p0p1.Cross(p1p2).Length() > 1e-6f;

	// 平面の方程式を作る（回転後の3頂点から）
	if (m_planevalid) {
		m_plane.MakeEquatation(m_rotatedvertices[0], m_rotatedvertices[1], m_rotatedvertices[2]);
	}

	// ---------------------------------------------
	// 直線と平面の交点を求める
	// ---------------------------------------------
	// 直線の方向ベクトル（t=0 で開始点、t=1 で終了点）
	Vector3 linedir = m_lineend - m_linestart;

	// 直線上の点 P(t) = 開始点 + t × 方向ベクトル
	Vector3 linepos = m_linestart + linedir * m_linet;

	m_hitvalid = false;
	if (m_planevalid) {
		Plane pl = m_plane.GetPlaneInfo().plane;
		Vector3 normal(pl.a, pl.b, pl.c);

		// 平面の方程式 n・P + D = 0 に P = S + tV を代入すると
		// n・S + t(n・V) + D = 0 → t = -(n・S + D) / (n・V)
		float denom = normal.Dot(linedir);

		// 分母が0なら直線と平面は平行（交点なし）
		if (std::fabs(denom) > 1e-6f) {
			m_hitt = -(normal.Dot(m_linestart) + pl.d) / denom;
			m_hitpos = m_linestart + linedir * m_hitt;
			m_hitvalid = true;
		}
	}

	// 3頂点を球で表示
	Color vertexcol[3] = {
		Color(1, 0, 0, 1),
		Color(0, 1, 0, 1),
		Color(0, 0, 1, 1)
	};

	for (int cnt = 0; cnt < 3; cnt++)
	{
		PointDraw(m_rotatedvertices[cnt], 3.0f, vertexcol[cnt]);
	}

	// 三角形の3辺を表示
	LineDraw(m_rotatedvertices[0], m_rotatedvertices[1], Color(1, 1, 1, 1));
	LineDraw(m_rotatedvertices[1], m_rotatedvertices[2], Color(1, 1, 1, 1));
	LineDraw(m_rotatedvertices[2], m_rotatedvertices[0], Color(1, 1, 1, 1));

	// 直線を表示（開始点 → P(t)）
	PointDraw(m_linestart, 2.0f, Color(1, 0, 1, 1));		// 開始点（マゼンタ）
	PointDraw(m_lineend, 1.5f, Color(0, 1, 1, 1));			// 終了点（シアン）
	LineDraw(m_linestart, linepos, Color(1, 0.5f, 0, 1), 3.0f);
	PointDraw(linepos, 1.5f, Color(1, 0.5f, 0, 1));			// P(t)（オレンジ）

	// 交点を球で表示
	if (m_hitvalid) {
		// 描画中の線分（開始点～P(t)）上にあれば赤、延長線上なら紫
		float tmin = m_linet < 0.0f ? m_linet : 0.0f;
		float tmax = m_linet < 0.0f ? 0.0f : m_linet;
		bool onsegment = (m_hitt >= tmin && m_hitt <= tmax);

		PointDraw(m_hitpos, 3.0f, onsegment ? Color(1, 0, 0, 1) : Color(0.6f, 0, 1, 1));
	}

	if (!m_planevalid) return;

	PLANEINFO info = m_plane.GetPlaneInfo();

	// 法線ベクトルを表示（重心から法線方向へ）
	Vector3 center = (info.p0 + info.p1 + info.p2) / 3.0f;
	Vector3 normal(info.plane.a, info.plane.b, info.plane.c);
	Vector3 normalend = center + normal * 30.0f;

	LineDraw(center, normalend, Color(1, 1, 0, 1), 3.0f);
	PointDraw(normalend, 1.5f, Color(1, 1, 0, 1));

	// 平面を表示（半透明なので最後に描く）
	PlaneDraw(info, 500.0f, Color(0.3f, 0.8f, 1.0f, 0.4f));
}

/*
 * @brief シーンの初期化処理
 */
void Game3DScene::init()
{
	// カメラ(3D)の初期化
	m_camera = std::make_unique <FreeCamera>(
		Vector3(0,30,-100),						// カメラ位置
		Vector3(0,0,0),							// 注視点
		Vector3(0,1,0));						// 上向きベクトル

	// DirectWrite生成
	m_directwrite = std::make_unique<DirectWrite>(&m_fontdata);
	m_directwrite->Init(Renderer::GetSwapChain());

	{
		// 軸表示用線分の初期化
		m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
		m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
		m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));
	}

	{
		SphereDrawerInit();
		BoxDrawerInit();
		CylinderDrawerInit();
		ConeDrawerInit(true);
		LineDrawerInit();
		CapsuleDrawerInit();
		PlaneDrawerInit();
	}

	// リソースを読み込む
	resourceLoader();

	// 平面を作る3頂点の初期値
	m_planevertices[0] = Vector3(-40, 0, 0);
	m_planevertices[1] = Vector3(0, 0, 40);
	m_planevertices[2] = Vector3(40, 10, 0);

	// 平面の回転角度の初期値（度）
	m_planerotation = Vector3(0, 0, 0);

	// 直線の開始点・終了点・媒介変数の初期値
	m_linestart = Vector3(-20, 60, -20);
	m_lineend = Vector3(20, -40, 20);
	m_linet = 1.0f;

	// デバッグUIにフリーカメラのパラメータを表示する関数を登録
	DebugUI::RedistDebugFunction([this]() {
		DebugFreeCamera();
		DebugPlane();
		DebugLine();
		});

}

/**
 * @brief シーンの終了処理
 */
void Game3DScene::dispose()
{
}

/**
 * @brief リソースを読み込む
 */
void Game3DScene::resourceLoader()
{
	{
		// シェーダを生成
		std::unique_ptr<CShader> shader{};
		shader = std::make_unique<CShader>();
		shader->Create(
			"shader/vertexLightingVS.hlsl",			// 頂点シェーダー
			"shader/vertexLightingPS.hlsl"			// ピクセルシェーダー
		);
		ShaderManager::RegisterShader<CShader>("Shader3D", std::move(shader));
	}

}

// フリーカメラのデバッグUI
void Game3DScene::DebugFreeCamera() {

	struct camerastate {
		float cameraradius;
		float azimuth;
		float elevation;
	};

	static camerastate state = { 100.0f, 0.0f, -(PI / 180.0f) * 80.0f };

	ImGui::Begin("Debug Free Camera Parameter");

	ImGui::SliderFloat("Radius", &state.cameraradius, 10.0f, 500.0f);
	ImGui::SliderFloat("Azimuth", &state.azimuth, 0.0f, 2 * PI);
	ImGui::SliderFloat("Elevation", &state.elevation, -PI, PI);

	FreeCamera* freecam = dynamic_cast<FreeCamera*>(m_camera.get());

	freecam->SetRadius(state.cameraradius);
	freecam->SetAzimuth(state.azimuth);
	freecam->SetElevation(state.elevation);

	freecam->Update();

	ImGui::End();
}

// 平面のデバッグUI
void Game3DScene::DebugPlane() {

	ImGui::Begin("Debug Plane Parameter");

	// 3頂点をドラッグで編集（回転前の座標）
	ImGui::DragFloat3("p0", &m_planevertices[0].x, 1.0f, -500.0f, 500.0f);
	ImGui::DragFloat3("p1", &m_planevertices[1].x, 1.0f, -500.0f, 500.0f);
	ImGui::DragFloat3("p2", &m_planevertices[2].x, 1.0f, -500.0f, 500.0f);
	ImGui::Separator();

	// 平面の回転（XYZ回転角度・度）
	ImGui::SliderFloat("rot X (deg)", &m_planerotation.x, -180.0f, 180.0f);
	ImGui::SliderFloat("rot Y (deg)", &m_planerotation.y, -180.0f, 180.0f);
	ImGui::SliderFloat("rot Z (deg)", &m_planerotation.z, -180.0f, 180.0f);
	if (ImGui::Button("Reset Rotation")) {
		m_planerotation = Vector3(0, 0, 0);
	}
	ImGui::Separator();

	// 回転後の3頂点
	for (int cnt = 0; cnt < 3; cnt++) {
		const Vector3& p = m_rotatedvertices[cnt];
		ImGui::Text("rotated p%d : (%.3f, %.3f, %.3f)", cnt, p.x, p.y, p.z);
	}
	ImGui::Separator();

	if (!m_planevalid) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "3 points are on a line. (no plane)");
		ImGui::End();
		return;
	}

	PLANEINFO info = m_plane.GetPlaneInfo();
	Plane pl = info.plane;

	// 法線ベクトル
	ImGui::Text("normal : (%.3f, %.3f, %.3f)", pl.a, pl.b, pl.c);
	ImGui::Separator();

	// 平面方程式の係数
	ImGui::Text("A = %.3f", pl.a);
	ImGui::Text("B = %.3f", pl.b);
	ImGui::Text("C = %.3f", pl.c);
	ImGui::Text("D = %.3f", pl.d);
	ImGui::Text("%.3fx + %.3fy + %.3fz + %.3f = 0", pl.a, pl.b, pl.c, pl.d);
	ImGui::Separator();

	// 確認：回転後の3頂点を方程式に代入すると 0 になる
	for (int cnt = 0; cnt < 3; cnt++) {
		const Vector3& p = m_rotatedvertices[cnt];
		float val = pl.a * p.x + pl.b * p.y + pl.c * p.z + pl.d;
		ImGui::Text("p%d : Ax+By+Cz+D = %.5f", cnt, val);
	}

	ImGui::End();
}

// 直線と平面の交点のデバッグUI
void Game3DScene::DebugLine() {

	ImGui::Begin("Debug Line Plane Intersection");

	// 開始点・終了点をドラッグで編集
	ImGui::DragFloat3("start", &m_linestart.x, 1.0f, -500.0f, 500.0f);
	ImGui::DragFloat3("end", &m_lineend.x, 1.0f, -500.0f, 500.0f);
	ImGui::Separator();

	// 媒介変数 t（スライダーでも数値入力でも変更できる）
	ImGui::SliderFloat("t (slider)", &m_linet, -3.0f, 3.0f);
	ImGui::InputFloat("t (input)", &m_linet, 0.1f, 1.0f, "%.3f");
	ImGui::Separator();

	// 直線の方程式 P(t) = S + t(E - S)
	Vector3 linedir = m_lineend - m_linestart;
	Vector3 linepos = m_linestart + linedir * m_linet;

	ImGui::Text("P(t) = S + t * V");
	ImGui::Text("S = (%.3f, %.3f, %.3f)", m_linestart.x, m_linestart.y, m_linestart.z);
	ImGui::Text("V = (%.3f, %.3f, %.3f)", linedir.x, linedir.y, linedir.z);
	ImGui::Text("P(%.3f) = (%.3f, %.3f, %.3f)", m_linet, linepos.x, linepos.y, linepos.z);
	ImGui::Text("length = %.3f", (linepos - m_linestart).Length());
	ImGui::Separator();

	if (!m_planevalid) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "3 points are on a line. (no plane)");
		ImGui::End();
		return;
	}

	// 平面の方程式
	Plane pl = m_plane.GetPlaneInfo().plane;
	ImGui::Text("plane : %.3fx + %.3fy + %.3fz + %.3f = 0", pl.a, pl.b, pl.c, pl.d);
	ImGui::Separator();

	if (!m_hitvalid) {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "line is parallel to plane. (no intersection)");
		ImGui::End();
		return;
	}

	// 交点
	ImGui::Text("t (hit) = %.5f", m_hitt);
	ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "intersection : (%.3f, %.3f, %.3f)",
		m_hitpos.x, m_hitpos.y, m_hitpos.z);

	// 確認：交点を平面の方程式に代入すると 0 になる
	float val = pl.a * m_hitpos.x + pl.b * m_hitpos.y + pl.c * m_hitpos.z + pl.d;
	ImGui::Text("check : Ax+By+Cz+D = %.5f", val);

	// 描画中の線分（t=0 ～ 現在の t）上に交点があるか
	float tmin = m_linet < 0.0f ? m_linet : 0.0f;
	float tmax = m_linet < 0.0f ? 0.0f : m_linet;
	if (m_hitt >= tmin && m_hitt <= tmax) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "hit on drawn segment (red sphere)");
	}
	else {
		ImGui::TextColored(ImVec4(0.8f, 0.5f, 1, 1), "hit on extended line (purple sphere)");
	}

	ImGui::End();
}

#include "../gameobject/field.h"
#include "../gameobject/player.h"
#include "../gameobject/wall.h"
#include "Game3DScene.h"
#include <array>
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
#include "../system/RandomEngine.h"

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

	// 3軸カラー
	Color axiscol[3] = {
		Color(1, 0, 0, 1),
		Color(0, 1, 0, 1),
		Color(0, 1, 1, 1)
	};

	// 3本のworld軸を描画
	for (int cnt = 0; cnt < m_segments.size(); cnt++)
	{
		Matrix4x4 worldmtx = Matrix4x4::Identity;
		m_segments[cnt]->SetWidth(1.0f);
		m_segments[cnt]->Draw(worldmtx, Color(0, 0, 0, 1));
	}

	// 3本のロ－カル軸を描画
	player* p = GetGameObject<player>();
	SRT srt = p->getSRT();
	srt.scale = Vector3(1, 1, 1);
	Matrix4x4 worldmtx = srt.GetMatrix();
	for (int cnt = 0; cnt < m_segments.size(); cnt++)
	{
		m_segments[cnt]->SetWidth(1.0f);
		m_segments[cnt]->Draw(worldmtx, axiscol[cnt]);
	}

	for (int layer = 0; layer < LAYERMAX; ++layer)
	{
		auto& objects = m_gameobjects[layer];

		for (auto& obj : objects)
		{
			if (obj && !obj->isDestroyed())
			{
				obj->draw(deltatime);
			}
		}
	}

	// フィールドとプレイヤを取得
	player* playerobj = GetGameObject<player>();

	Vector3 ppos = playerobj->getSRT().pos;
	ppos.y = 0.0f;	// プレイヤのY座標は地面に合わせる

	field* fieldobj = GetGameObject<field>();
	std::array<Vector3, 3> currenttriangle = fieldobj->GetCurrentTriangleVertex(ppos);

	// 3頂点を球で表示
	Color vertexcol[3] = {
		Color(1, 0, 0, 1),
		Color(0, 1, 0, 1),
		Color(0, 0, 1, 1)
	};

	for (int cnt = 0; cnt < 3; cnt++)
	{
		::PointDraw(currenttriangle[cnt], 3.0f, vertexcol[cnt]);
	}

	// 三角形の3辺を表示
	::LineDraw(currenttriangle[0], currenttriangle[1], Color(1, 1, 1, 1));
	::LineDraw(currenttriangle[1], currenttriangle[2], Color(1, 1, 1, 1));
	::LineDraw(currenttriangle[2], currenttriangle[0], Color(1, 1, 1, 1));

	{
		// プレイヤのBSを表示
		GM31::GE::Collision::BoundingSphere bs;
		player* pobj = GetGameObject<player>();
		SRT srt = pobj->getSRT();
		bs.center = srt.pos;
		bs.radius = 20.0f;

		::BSDraw(bs, Color(1, 0, 0, 0.3f));

		// 壁のOBBを表示
		GM31::GE::Collision::BoundingBoxOBB obb;

		std::vector<wall*> wallobjs = GetGameObjects<wall>();

		for (auto* wallobj : wallobjs) {
			SRT wallsrt = wallobj->getSRT();

			obb.lengthx = wallobj->getwidth();
			obb.lengthy = wallobj->getheight();
			obb.lengthz = wallobj->getthick();

			obb.worldcenter = wallsrt.pos;
			wallsrt.scale = Vector3(1, 1, 1);
			Matrix4x4 mtx = wallsrt.GetMatrix();
			obb.axisX = mtx.Right();
			obb.axisY = mtx.Up();
			obb.axisZ = mtx.Forward();

			{
				bool sts = GM31::GE::Collision::CollisionSphereOBB(bs,obb);
				if (sts) {
					OBBDraw(obb,Color(1,0,0,0.3f));
				}
				else {
					OBBDraw(obb, Color(1, 1, 1, 0.3f));
				}
			}
		}
	}
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

	// プレイヤオブジェクト生成
	{
		player* playerObj = addGameObject<player>(0);
		playerObj->setmeshid("car000.x");
	}

	// フィールドジェクト生成
	{
		field* fieldObj = addGameObject<field>(0);
	}

	// 壁オブジェクト生成
	{
		RandomEngine randeng{};
		for (int wallcnt = 0; wallcnt < WALLNUM; wallcnt++) {
			{
				wall* wallObj = addGameObject<wall>(0);
				wallObj->setheight(200);
				wallObj->setwidth(300);
				wallObj->setthick(1.0f);
				wallObj->setPosition(Vector3(
					randeng.uniformRealfloat(-500, 500),
					0,
					randeng.uniformRealfloat(-500, 500)
				));	
				wallObj->setRotationY(randeng.uniformRealfloat(-PI, PI));
				wallObj->calcEqation();
			}
		}
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
	m_planevertices[0] = Vector3(-40, 0.1f, 0);
	m_planevertices[1] = Vector3(0, 0.1f, 40);
	m_planevertices[2] = Vector3(-40, 0.1f, 40);

	// 平面の回転角度の初期値（度）
	m_planerotation = Vector3(0, 0, 0);

	// 直線の開始点・終了点・媒介変数の初期値
	m_linestart = Vector3(0, -10, -0);
	m_lineend = Vector3(0, 10, 0);
	m_linet = 1.0f;

	// デバッグUIにフリーカメラのパラメータを表示する関数を登録
	DebugUI::RedistDebugFunction([this]() {
		DebugFreeCamera();
//		DebugPlane();
//		DebugLine();
		DebugWall();
		DebugFieldUnduration();
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

	// メッシュを生成
	for (int cnt = 0; cnt < g_loadmodel.size(); cnt++) {
		std::unique_ptr<CStaticMesh> mesh{};
		mesh = std::make_unique<CStaticMesh>();
		mesh->Load(g_loadmodel[cnt].filename, g_loadmodel[cnt].texdirectoryname);

		// メッシュレンダラを生成
		std::unique_ptr<CStaticMeshRenderer> meshrenderer{};
		meshrenderer = std::make_unique<CStaticMeshRenderer>();
		meshrenderer->Init(*mesh.get());

		g_loadmodel[cnt].meshid = getfilename(g_loadmodel[cnt].filename);

		MeshManager::RegisterMesh<CStaticMesh>(g_loadmodel[cnt].meshid, std::move(mesh));
		MeshManager::RegisterMeshRenderer<CStaticMeshRenderer>(g_loadmodel[cnt].meshid, std::move(meshrenderer));
	}
}

// 壁のデバッグUI
void Game3DScene::DebugWall() {

	struct Wallstate {
		float height;
		float width;
		Vector3 position;
		float roty;
	};

	static Wallstate wallstate = {100,200,Vector3(0,0,0),0};

	ImGui::Begin("debug Walls");

	ImGui::SliderFloat("height", &wallstate.height, 10.0f, 500.0f);
	ImGui::SliderFloat("width", &wallstate.width, 10.0f, 1000.0f);
	ImGui::SliderFloat("rotation Y", &wallstate.roty, -PI, PI);

	ImGui::DragFloat3("position", &wallstate.position.x, 1.0f,-1000, 1000);

	wall* wobj = GetGameObject<wall>();

	wobj->setwidth(wallstate.width);
	wobj->setheight(wallstate.height);
	wobj->setPosition(wallstate.position);
	wobj->setthick(1.0f);
	SRT srt = wobj->getSRT();
	srt.rot.y = wallstate.roty;
	wobj->setSRT(srt);

	wobj->calcEqation();
	ImGui::End();
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

	// 直線を平面と平行にする（開始点はそのままで、終了点を動かす）
	if (ImGui::Button("Make Line Parallel to Plane")) {
		Vector3 normal(pl.a, pl.b, pl.c);
		float linelength = linedir.Length();

		// 方向ベクトルから法線方向の成分を取り除く（平面へ射影する）
		// V' = V - (n・V) n
		Vector3 newdir = linedir - normal * normal.Dot(linedir);

		// 直線が平面に垂直だった場合は射影すると長さ0になるので、平面上の辺の方向を使う
		if (newdir.Length() < 1e-4f * (linelength > 1.0f ? linelength : 1.0f)) {
			newdir = m_rotatedvertices[1] - m_rotatedvertices[0];
		}

		// 元の長さを保つ（元の長さが0なら100にする）
		newdir.Normalize();
		newdir *= (linelength > 0.0f ? linelength : 100.0f);

		m_lineend = m_linestart + newdir;
	}
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
	ImGui::Separator();

	// 三角形の内外判定
	if (m_hitintriangle) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "INSIDE triangle (big sphere)");
		ImGui::TextColored(ImVec4(0, 1, 0, 1), "inside hit : (%.3f, %.3f, %.3f)",
			m_hitpos.x, m_hitpos.y, m_hitpos.z);
	}
	else {
		ImGui::TextColored(ImVec4(1, 0.6f, 0, 1), "OUTSIDE triangle");
	}

	ImGui::End();
}

// フィールドに凸凹にする
void Game3DScene::DebugFieldUnduration() {

	ImGui::Begin("debug Field Remake with unduration");

	static float minheight = 0.0f;
	static float maxheight = 100.0f;

	ImGui::SliderFloat("low height", &minheight, 0.0f, 10.0f);
	ImGui::SliderFloat("max hight", &maxheight, 0.0f, 100.0f);

	static float perlinscale = 0.5f;     // ノイズの細かさ（お好みで 0.02～0.2 くらい）
	static float perlinoffsetX = 10.0f;   // シード代わりのオフセット（任意）
	static float perlinoffsetZ = 10.0f;

	ImGui::SliderFloat("perlin scale", &perlinscale, 0.0f, 5.0f);
	ImGui::SliderFloat("perlin offset x", &perlinoffsetX, 10.0f, 800.0f);
	ImGui::SliderFloat("perlin offset z", &perlinoffsetZ, 10.0f, 800.0f);


	if (ImGui::Button("remake field with perlin")) {

		field* fieldobj = GetGameObject<field>();
		fieldobj->makeundulationwithperlin(minheight, minheight + maxheight,
			perlinscale, perlinoffsetX, perlinoffsetZ);
	}

	ImGui::End();
}
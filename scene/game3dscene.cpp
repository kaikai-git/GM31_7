#include "../gameobject/field.h"
#include "../gameobject/player.h"
#include "../gameobject/wall.h"
#include "Game3DScene.h"
#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "../system/C3DShape.h"
#include "../system/camera.h"
#include "../system/collision.h"
#include "../system/commontypes.h"
#include "../system/DebugUI.h"
#include "../system/DirectWrite.h"
#include "../system/renderer.h"
#include "../system/RandomEngine.h"

namespace {

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
    for (auto& objects : m_gameobjects) {
        for (auto& object : objects) {
            if (object && !object->isDestroyed()) object->update(deltatime);
        }
    }
    for (auto& objects : m_gameobjects) {
        std::erase_if(objects, [](const auto& object) { return !object || object->isDestroyed(); });
    }
    for (int layer = 0; layer < LAYERMAX; ++layer) {
        for (auto& object : m_pendingAddObjects[layer]) m_gameobjects[layer].push_back(std::move(object));
        m_pendingAddObjects[layer].clear();
    }
    UpdateFollowCamera();
}

/**
 * @brief 描画処理
 *
 * @param deltatime 前フレームからの経過時間（ミリ秒）
 */

void Game3DScene::draw(uint64_t deltatime)
{
	m_camera->Draw();

    if (m_showAxes) {
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

    if (m_showTerrainTriangle) {
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

    }

	{
        player* pobj = GetGameObject<player>();
        if (m_showPlayerBounds) {
            const Matrix4x4 bounds = Matrix4x4::CreateTranslation(
                pobj->getSRT().pos + Vector3(0, player::MODEL_HEIGHT * 0.5f, 0));
            CapsuleDrawerDraw(player::COLLISION_RADIUS,
                player::MODEL_HEIGHT - 2 * player::COLLISION_RADIUS, Color(1, 0, 0, 0.2f), bounds);
        }

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
				bool sts = pobj->overlapsWall(*wallobj);
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
    SetWindowTextW(Application::GetWindow(), L"GM31 No.7 - Character Gameplay (Video 2)");
    m_camera = std::make_unique<FreeCamera>(Vector3(0, 140, -240),
        Vector3(0, player::MODEL_HEIGHT * 0.5f, 0), Vector3(0, 1, 0));
    m_segments[0] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(100, 0, 0));
    m_segments[1] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 100, 0));
    m_segments[2] = std::make_unique<Segment>(Vector3(0, 0, 0), Vector3(0, 0, 100));

    field* terrain = addGameObject<field>(0);
    terrain->makeundulationwithperlin(0, 30, 0.35f, 10, 10);
    player* character = addGameObject<player>(0);
    character->setPosition(Vector3(0, terrain->GetHeight2(Vector3(0, 0, 0)), 0));

    RandomEngine random;
    for (uint32_t i = 0; i < WALLNUM; ++i) {
        wall* object = addGameObject<wall>(0);
        Vector3 position{};
        float height = 100;
        float width = 260;
        float angle = 0;
        if (i == 0) {
            position = Vector3(0, 0, 180);
        } else if (i == 1) {
            // A low fence to demonstrate jumping over an obstacle.
            position = Vector3(150, 0, 0);
            height = 24;
            width = 180;
            angle = PI * 0.5f;
        } else {
            for (int attempt = 0; attempt < 32; ++attempt) {
                position = Vector3(random.uniformRealfloat(-400, 400), 0, random.uniformRealfloat(-400, 400));
                if (position.LengthSquared() >= 250 * 250) break;
                position = Vector3(350, 0, 350);
            }
            angle = random.uniformRealfloat(-PI, PI);
        }
        position.y = terrain->GetHeight2(position) + height * 0.5f;
        object->setheight(height);
        object->setwidth(width);
        object->setthick(3);
        object->setPosition(position);
        object->setRotationY(angle);
        object->calcEqation();
    }
    CapsuleDrawerInit();
    BoxDrawerInit();
    CylinderDrawerInit();
    ConeDrawerInit(true);
    LineDrawerInit();
    PlaneDrawerInit();
    DebugUI::RedistDebugFunction([this]() {
        DebugCharacter();
        DebugFreeCamera();
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


// 壁のデバッグUI
void Game3DScene::DebugWall() {
    ImGui::SetNextWindowPos(ImVec2(950, 250), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("debug Walls")) { ImGui::End(); return; }
    ImGui::Checkbox("Show player capsule", &m_showPlayerBounds);
    wall* object = GetGameObject<wall>();
    if (object) {
        float height = object->getheight();
        float width = object->getwidth();
        SRT transform = object->getSRT();
        bool changed = ImGui::SliderFloat("height", &height, 10, 300);
        changed |= ImGui::SliderFloat("width", &width, 10, 500);
        changed |= ImGui::SliderFloat("rotation Y", &transform.rot.y, -PI, PI);
        changed |= ImGui::DragFloat3("position", &transform.pos.x, 1, -500, 500);
        if (changed) {
            object->setheight(height);
            object->setwidth(width);
            object->setSRT(transform);
            object->calcEqation();
        }
    }
    ImGui::End();
}

// フリーカメラのデバッグUI
void Game3DScene::DebugFreeCamera() {
    ImGui::SetNextWindowPos(ImVec2(950, 12), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    ImGui::Begin("Gameplay Camera");
    ImGui::SliderFloat("Radius", &m_cameraRadius, 150, 400);
    ImGui::SliderFloat("Azimuth", &m_cameraAzimuth, 0, 2 * PI);
    ImGui::SliderFloat("Elevation", &m_cameraElevation, -1.5f, -0.3f);
    ImGui::Checkbox("Show axes", &m_showAxes);
    ImGui::Checkbox("Show terrain triangle", &m_showTerrainTriangle);
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

	ImGui::SetNextWindowPos(ImVec2(950, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
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

void Game3DScene::UpdateFollowCamera() {
    const player* character = GetGameObject<player>();
    field* terrain = GetGameObject<field>();
    if (!character || !terrain) return;
    Vector3 target = character->getSRT().pos;
    // Follow ground height, so the character's jump remains visible on screen.
    target.y = terrain->GetHeight2(target) + player::MODEL_HEIGHT * 0.5f;
    CPolor3D offset(m_cameraRadius, m_cameraElevation, m_cameraAzimuth);
    m_camera->SetLookat(target);
    m_camera->SetPosition(target + offset.ToCartesian());
    m_camera->SetUP(Vector3(0, 1, 0));
}

void Game3DScene::DebugCharacter() {
    ImGui::SetWindowPos("Debug Information", ImVec2(12, 640), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
    ImGui::Begin("Character Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::TextUnformatted("W A S D : Walk     Space : Jump");
    ImGui::TextUnformatted("J : Horizontal slash   K : Vertical slash");
    ImGui::TextUnformatted("R : Reload saved .gmotion files");
    if (const player* character = GetGameObject<player>()) {
        const auto& motion = character->animation();
        const char* attack = motion.CurrentAttack() == Motion::Attack::Horizontal ? "Horizontal"
            : (motion.CurrentAttack() == Motion::Attack::Vertical ? "Vertical" : "None");
        ImGui::Text("Motion: %s   Attack: %s", motion.LocomotionName(), attack);
        const Vector3 position = character->getSRT().pos;
        ImGui::Text("Position: %.1f, %.1f, %.1f", position.x, position.y, position.z);
        ImGui::Text("Speed: %.1f   Grounded: %s   Wall: %s", character->getSpeed(),
            motion.Grounded() ? "Yes" : "No", character->isHitWall() ? "Yes" : "No");
        ImGui::PushTextWrapPos(370);
        ImGui::TextColored(character->motionsLoaded() ? ImVec4(0.5f, 1, 0.65f, 1) : ImVec4(1, 0.6f, 0.3f, 1),
            "%s", character->motionStatus().c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
}

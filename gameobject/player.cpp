#include <algorithm>
#include <cmath>
#include <cstdint>
#include	"player.h"	
#include	"wall.h"
#include	"field.h"
#include	"../system/commontypes.h"
#include	"../system/Inputmanager.h"	
#include	"../system/transform.h"
#include "../system/CapsuleDrawer.h"
#include "../system/SphereDrawer.h"
#include	"../system/camera.h"
#include	"../system/collision.h"

namespace {
    // Capsule endpoints are sphere centers in the player's local space.
    void DrawBodyPart(const Vector3& start, const Vector3& end, float radius,
        const Color& color, const Matrix4x4& playerWorld)
    {
        Vector3 up = end - start;
        const float length = up.Length();
        if (length <= 0.0001f) {
            return;
        }
        up /= length;

        const Vector3 reference = std::fabs(up.z) < 0.99f
            ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
        Vector3 right = up.Cross(reference);
        right.Normalize();
        const Vector3 forward = right.Cross(up);
        const Vector3 center = (start + end) * 0.5f;

        Matrix4x4 local = Matrix4x4::Identity;
        local._11 = right.x;   local._12 = right.y;   local._13 = right.z;
        local._21 = up.x;      local._22 = up.y;      local._23 = up.z;
        local._31 = forward.x; local._32 = forward.y; local._33 = forward.z;
        local._41 = center.x;  local._42 = center.y;  local._43 = center.z;

        CapsuleDrawerDraw(radius, length, color, local * playerWorld);
    }

    Vector3 SwingOffset(const Vector3& offset, float angle)
    {
        return Vector3::TransformNormal(offset, Matrix4x4::CreateRotationX(angle));
    }
}

void player::init() {

	m_srt.pos = Vector3(0, 0, 0);
	m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
	m_srt.rot = Vector3(0, 0, 0);
    m_walkPhase = 0.0f;
    m_walkAmount = 0.0f;
}

void player::update(uint64_t dt) {

	Camera* camera = m_ownerscene->getcamera();
	FreeCamera* freecamera = dynamic_cast<FreeCamera*>(camera);
	if (freecamera == nullptr) { return; }

	Matrix4x4 viewmtx = freecamera->GetViewMatrix();

	// ビュー行列からカメラの前方向・右方向を取得し、XZ平面に投影する
	Vector3 camforward = Vector3(viewmtx._13, 0.0f, viewmtx._33);
	Vector3 camright = Vector3(viewmtx._11, 0.0f, viewmtx._31);

	// 正規化(真上/真下を向いている場合の保険つき)
	float lenf = std::sqrtf(camforward.x * camforward.x + camforward.z * camforward.z);
	if (lenf > 0.0001f)
	{
		camforward.x /= lenf;
		camforward.z /= lenf;
	}
	else
	{
		camforward = Vector3(0.0f, 0.0f, 1.0f);
	}

	float lenr = std::sqrtf(camright.x * camright.x + camright.z * camright.z);
	if (lenr > 0.0001f)
	{
		camright.x /= lenr;
		camright.z /= lenr;
	}
	else
	{
		camright = Vector3(1.0f, 0.0f, 0.0f);
	}

	// 入力をカメラ基準の移動方向に合成する
	float dirx = 0.0f;
	float dirz = 0.0f;

	if (CInputManager::GetInstance().IsKeyPressed(DIK_W))
	{// 前移動(カメラの奥方向)
		dirx += camforward.x;
		dirz += camforward.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_S))
	{// 後移動(カメラの手前方向)
		dirx -= camforward.x;
		dirz -= camforward.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_A))
	{// 左移動(カメラの左方向)
		dirx -= camright.x;
		dirz -= camright.z;
	}
	if (CInputManager::GetInstance().IsKeyPressed(DIK_D))
	{// 右移動(カメラの右方向)
		dirx += camright.x;
		dirz += camright.z;
	}

	// 移動入力があれば正規化して移動量と目標角度をセット
	float lend = std::sqrtf(dirx * dirx + dirz * dirz);
	if (lend > 0.0001f)
	{
		dirx /= lend;
		dirz /= lend;

		m_move.x += dirx * VALUE_MOVE_MODEL;
		m_move.z += dirz * VALUE_MOVE_MODEL;

		// 移動方向 (-sin, -cos) から目標角度を逆算する
		m_destrot.y = std::atan2f(-dirx, -dirz);
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_RIGHT))
	{// 左回転
		m_destrot.y = m_srt.rot.y - VALUE_ROTATE_MODEL;
		if (m_destrot.y < -PI)
		{
			m_destrot.y += PI * 2.0f;
		}
	}

	if (CInputManager::GetInstance().IsKeyPressed(DIK_LEFT))
	{// 右回転
		m_destrot.y = m_srt.rot.y + VALUE_ROTATE_MODEL;
		if (m_destrot.y > PI)
		{
			m_destrot.y -= PI * 2.0f;
		}
	}

	// 目標角度と現在角度との差分を求める
	float diffrot = m_destrot.y - m_srt.rot.y;
	if (diffrot > PI)
	{
		diffrot -= PI * 2.0f;
	}
	if (diffrot < -PI)
	{
		diffrot += PI * 2.0f;
	}

	// 比率計算
	m_srt.rot.y += diffrot * RATE_ROTATE_MODEL;
	if (m_srt.rot.y > PI)
	{
		m_srt.rot.y -= PI * 2.0f;
	}
	if (m_srt.rot.y < -PI)
	{
		m_srt.rot.y += PI * 2.0f;
	}

	// 壁との当たり判定
	std::vector<wall*> wallobjs = m_ownerscene->GetGameObjects<wall>();

	// 移動前の座標（壁のどちら側にいたかの基準になる）
	const Vector3 basepos = m_srt.pos;

	// プレイヤの当たり判定用の球
	GM31::GE::Collision::BoundingSphere bs;
	bs.radius = COLLISION_RADIUS;
	bs.center = basepos + m_move;

	m_ishitwall = false;

	// 1つの壁から押し出された結果、別の壁にめり込むことがあるので
	// めり込みが無くなるまで（最大 MAX_WALL_RESOLVE 回）繰り返す
	bool ishitthisloop = false;

	for (int loopcnt = 0; loopcnt < MAX_WALL_RESOLVE; loopcnt++) {

		ishitthisloop = false;

		// 今の移動量での次フレーム位置
		Vector3 nextpos = basepos + m_move;

		for (const auto& wallobj : wallobjs) {
			CPlane pl = wallobj->getEquation();
			PLANEINFO plinfo = pl.GetPlaneInfo();

			// 壁（無限平面）までの符号付き距離で大まかに絞り込む
			float lng = GM31::GE::Collision::calcPlaneDistance(plinfo.plane, nextpos);
			if (fabsf(lng) >= bs.radius) {
				continue;		// 平面から十分離れている
			}

			// 壁の板（OBB）と球で判定する
			GM31::GE::Collision::BoundingBoxOBB obb;

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

			bs.center = nextpos;

			if (GM31::GE::Collision::CollisionSphereOBB(bs, obb) == false) {
				continue;		// 板の範囲外を通過している
			}

			// めり込みベクトルを求める
			Vector3 penetration = GM31::GE::Collision::calcPenetrationVector(
				plinfo.plane, basepos, nextpos, bs.radius);

			// ちょうど接しているだけなら押し出す必要は無い
			if (penetration.LengthSquared() <= WALL_EPSILON) {
				continue;
			}

			// 壁との交点を求める（デバッグ表示用）
			GM31::GE::Collision::calcWallCrossPoint(
				plinfo.plane, basepos, m_move, m_crosspoint);

			m_penetration = penetration;

			// 次フレーム位置をめり込みベクトル分だけ押し出す
			nextpos += penetration;

			// 押し出された新しい位置から、移動量(m_move)を逆算する
			// （これにより、自然に壁に沿った移動ベクトルになります）
			m_move = nextpos - basepos;

			ishitthisloop = true;
			m_ishitwall = true;
		}

		// どの壁ともめり込まなくなったら終了
		if (ishitthisloop == false) {
			break;
		}
	}

	// 繰り返しても解決しない（壁と壁に挟まれた）場合はその場で止まる
	if (ishitthisloop) {
		m_move = Vector3(0, 0, 0);
	}

	/// 位置移動
	m_srt.pos += m_move;

	// 地面との当たり判定（移動後の位置で高さを求める）
	field* fieldobj = m_ownerscene->GetGameObject<field>();

	float height = fieldobj->GetHeight2(m_srt.pos);
	m_srt.pos.y = height;

	    // Advance the gait from actual travel, so blocked movement does not walk in place.
    const float distance = std::sqrt(m_move.x * m_move.x + m_move.z * m_move.z);
    const float targetWalkAmount = std::min(distance / 8.0f, 1.0f);
    const float seconds = std::min(static_cast<float>(dt) / 1000000.0f, 0.1f);
    m_walkAmount += (targetWalkAmount - m_walkAmount) * std::min(seconds * 12.0f, 1.0f);
    m_walkPhase = std::fmod(m_walkPhase + distance * 0.12f, 2.0f * PI);

    // 移動量に慣性をかける(減速率)//
	m_move += -m_move * RATE_MOVE_MODEL;
}
	
void player::draw(uint64_t /*dt*/) {
    const Matrix4x4 world = m_srt.GetMatrix();
    const Color shirt(0.12f, 0.48f, 0.85f, 1.0f);
    const Color trousers(0.13f, 0.19f, 0.30f, 1.0f);
    const Color skin(1.0f, 0.76f, 0.55f, 1.0f);
    const Color shoes(0.08f, 0.09f, 0.12f, 1.0f);
    const Color eyes(0.04f, 0.05f, 0.07f, 1.0f);

    // Feet rest on local Y=0; the head reaches MODEL_HEIGHT.
    DrawBodyPart(Vector3(-6, 35, 0), Vector3(6, 35, 0), 7.0f, trousers, world);
    DrawBodyPart(Vector3(0, 43, 0), Vector3(0, 55, 0), 10.0f, shirt, world);
    DrawBodyPart(Vector3(0, 62, 0), Vector3(0, 66, 0), 3.5f, skin, world);
    DrawBodyPart(Vector3(0, 71, 0), Vector3(0, 75, 0), 8.0f, skin, world);

    for (int side : {-1, 1}) {
        const float wave = std::sin(m_walkPhase) * static_cast<float>(side);
        const float legAngle = wave * 0.45f * m_walkAmount;
        const float kneeBend = std::max(wave, 0.0f) * 0.55f * m_walkAmount;

        const Vector3 hip(side * 6.0f, 35, 0);
        const Vector3 knee = hip + SwingOffset(Vector3(0, -14, 0), legAngle);
        const Vector3 ankle = knee + SwingOffset(Vector3(0, -14, 0), legAngle - kneeBend);
        DrawBodyPart(hip, knee, 4.5f, trousers, world);
        DrawBodyPart(knee, ankle, 4.0f, trousers, world);

        const Vector3 heel = ankle + Vector3(0, -3, 1);
        DrawBodyPart(heel, heel + Vector3(0, 0, -7), 4.0f, shoes, world);

        // Arms swing opposite to the leg on the same side.
        const float armAngle = -wave * 0.5f * m_walkAmount;
        const Vector3 shoulder(side * 13.0f, 55, 0);
        const Vector3 elbow = shoulder + SwingOffset(Vector3(side * 3.0f, -12, 0), armAngle);
        const Vector3 wrist = elbow + SwingOffset(Vector3(side * 1.0f, -11, -1), armAngle - 0.12f);
        DrawBodyPart(shoulder, elbow, 3.5f, shirt, world);
        DrawBodyPart(elbow, wrist, 3.0f, skin, world);
        DrawBodyPart(wrist, wrist + Vector3(0, -3, 0), 3.5f, skin, world);

        // The face and shoes point toward local -Z, matching the movement heading.
        const Matrix4x4 eyeWorld = Matrix4x4::CreateScale(1.2f)
            * Matrix4x4::CreateTranslation(side * 2.8f, 74, -7.4f) * world;
        SphereDrawerDraw(eyeWorld, eyes);
    }
    DrawBodyPart(Vector3(0, 70, -7), Vector3(0, 70, -9), 1.5f, skin, world);
}

void player::dispose()
{

}

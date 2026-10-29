#include "field.h"
#include <cstdint>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <vector>
#include "../system/renderer.h"
#include "../system/stb_perlin.h"
#include "../system/collision.h"

namespace {
    // 高さ取得用のレイの向き（真上）
    const Vector3 RAY_UP = Vector3(0.0f, 1.0f, 0.0f);

    // 地面が見つからなかった時の高さ
    constexpr float DEFAULT_HEIGHT = 0.0f;
}

void field::init() {
    m_srt.pos = Vector3(0, 0, 0);
    m_srt.scale = Vector3(1.0f, 1.0f, 1.0f);
    m_srt.rot = Vector3(0, 0, 0);

    m_Shader.Create("shader/vertexLightingVS.hlsl", "shader/vertexLightingPS.hlsl");

    MATERIAL mtrl;
    mtrl.Ambient = Color(0, 0, 0, 0);
    mtrl.Diffuse = Color(1, 1, 1, 1);
    mtrl.Emission = Color(0, 0, 0, 0);
    mtrl.Specular = Color(0, 0, 0, 0);
    mtrl.Shiness = 0;
    mtrl.TextureEnable = true;
    m_Material.Create(mtrl);

    bool sts = m_Texture.Load("assets/texture/Grass01.jpg");
    assert(sts == true);

    m_planemesh.Init(m_divx, m_divz, m_sizex, m_sizez, Color(1, 1, 1, 1), Vector3(0, 1, 0), true);
    m_planerenderer.Init(m_planemesh);

    // 起伏をつける前（平らな状態）でも高さ取得できるようにしておく
    makeplaneequation();
}

void field::update(uint64_t /*dt*/) {}

void field::draw(uint64_t /*dt*/) {
    Matrix4x4 mtx = m_srt.GetMatrix();
    Renderer::SetWorldMatrix(&mtx);

    m_Shader.SetGPU();
    m_Material.SetGPU();
    m_Texture.SetGPU();

    m_planerenderer.Draw();
}

void field::dispose() {}

void field::makeundulationwithperlin(float minheight, float maxheight, float scale, float offsetx, float offsetz) {
    std::vector<VERTEX_3D> vertices = m_planemesh.GetVertices();
    const std::vector<uint32_t>& indices = m_planemesh.GetIndices();

    const int vertexcountx = m_divx + 1;    // 横一列分の頂点数

    // 頂点の高さをノイズで決める
    for (size_t i = 0; i < vertices.size(); ++i) {
        const int x = static_cast<int>(i) % vertexcountx;
        const int z = static_cast<int>(i) / vertexcountx;

        const float noisex = x * scale + offsetx;
        const float noisez = z * scale + offsetz;

        const float noise = stb_perlin_noise3(noisex, 0.0f, noisez, 0, 0, 0);   // [-1,1]
        const float rate = (noise + 1.0f) * 0.5f;                               // [0,1]

        vertices[i].Position.y = minheight + (maxheight - minheight) * rate;
    }

    // 高さが変わったので法線を計算し直す
    recalculatenormals(vertices, indices);

    m_planemesh.SetVertices(vertices);
    m_planerenderer.Modify(vertices);

    // 頂点が動いたので平面方程式も作り直す
    makeplaneequation();
}

void field::recalculatenormals(std::vector<VERTEX_3D>& vertices, const std::vector<uint32_t>& indices) {
    // いったん全部ゼロにする
    for (VERTEX_3D& vertex : vertices) {
        vertex.Normal = Vector3(0.0f, 0.0f, 0.0f);
    }

    const size_t trianglenum = indices.size() / 3;

    for (size_t i = 0; i < trianglenum; ++i) {
        const uint32_t idx0 = indices[i * 3 + 0];
        const uint32_t idx1 = indices[i * 3 + 1];
        const uint32_t idx2 = indices[i * 3 + 2];

        const Vector3 edge01 = vertices[idx1].Position - vertices[idx0].Position;
        const Vector3 edge02 = vertices[idx2].Position - vertices[idx0].Position;

        Vector3 facenormal = edge01.Cross(edge02);
        facenormal.Normalize();

        // 共有している頂点に面法線を足し込んでおく
        vertices[idx0].Normal += facenormal;
        vertices[idx1].Normal += facenormal;
        vertices[idx2].Normal += facenormal;
    }

    // 足し込んだ法線を平均化（正規化）する
    for (VERTEX_3D& vertex : vertices) {
        vertex.Normal.Normalize();
    }
}

void field::makeplaneequation() {
    const std::vector<VERTEX_3D>& vertices = m_planemesh.GetVertices();
    const std::vector<uint32_t>& indices = m_planemesh.GetIndices();

    const size_t trianglenum = indices.size() / 3;

    m_planeeq.clear();
    m_planeeq.reserve(trianglenum);

    for (size_t i = 0; i < trianglenum; ++i) {
        CPlane plane;
        plane.MakeEquatation(
            vertices[indices[i * 3 + 0]].Position,
            vertices[indices[i * 3 + 1]].Position,
            vertices[indices[i * 3 + 2]].Position);

        m_planeeq.emplace_back(plane);
    }
}

bool field::getplaneinfo(int triangleindex, PLANEINFO& planeinfo) const {
    if (triangleindex < 0 || triangleindex >= trianglecount()) {
        return false;
    }

    // 通常はキャッシュ（makeplaneequation の結果）から取り出す
    if (triangleindex < static_cast<int>(m_planeeq.size())) {
        planeinfo = m_planeeq[triangleindex].GetPlaneInfo();
        return true;
    }

    // キャッシュが無い場合の保険として、その場で作る
    const std::array<Vector3, 3> trianglevertices = GetFaceVertex(triangleindex);

    CPlane plane;
    plane.MakeEquatation(trianglevertices[0], trianglevertices[1], trianglevertices[2]);
    planeinfo = plane.GetPlaneInfo();

    return true;
}

bool field::raycastvertical(const Vector3& pos, int& triangleindex, Vector3& hitpoint) const {
    const int squareno = GetSquareNo(pos);
    if (squareno < 0) {
        return false;
    }

    const Vector3 startpoint = Vector3(pos.x, 0.0f, pos.z);

    bool ishit = false;

    // そのマスを構成する三角形だけ調べればよい
    for (int i = 0; i < TRIANGLES_PER_SQUARE; ++i) {
        const int no = squareno * TRIANGLES_PER_SQUARE + i;

        PLANEINFO planeinfo{};
        if (!getplaneinfo(no, planeinfo)) {
            continue;
        }

        float t = 0.0f;
        Vector3 crosspoint{};
        if (!GM31::GE::Collision::LinetoPlaneCross(planeinfo.plane, startpoint, RAY_UP, t, crosspoint)) {
            continue;
        }

        // 交点が三角形の内側にあれば確定
        if (GM31::GE::Collision::CheckInTriangle(planeinfo.p0, planeinfo.p1, planeinfo.p2, crosspoint)) {
            triangleindex = no;
            hitpoint = crosspoint;
            return true;
        }

        // マスの中にはいるので、境界の誤差で内側判定から外れた時は
        // 最後に交差した面を採用する
        triangleindex = no;
        hitpoint = crosspoint;
        ishit = true;
    }

    return ishit;
}

float field::GetHeight(const Vector3& pos) const {
    const Vector3 startpoint = Vector3(pos.x, 0.0f, pos.z);
    const int trianglenum = trianglecount();

    // 全三角形を総当たりで調べる（GetHeight2 の動作確認用）
    for (int i = 0; i < trianglenum; ++i) {
        PLANEINFO planeinfo{};
        if (!getplaneinfo(i, planeinfo)) {
            continue;
        }

        float t = 0.0f;
        Vector3 crosspoint{};
        if (!GM31::GE::Collision::LinetoPlaneCross(planeinfo.plane, startpoint, RAY_UP, t, crosspoint)) {
            continue;
        }

        if (GM31::GE::Collision::CheckInTriangle(planeinfo.p0, planeinfo.p1, planeinfo.p2, crosspoint)) {
            return crosspoint.y;
        }
    }

    return DEFAULT_HEIGHT;
}

float field::GetHeight2(const Vector3& pos) const {
    int triangleindex = -1;
    Vector3 hitpoint{};

    if (!raycastvertical(pos, triangleindex, hitpoint)) {
        return DEFAULT_HEIGHT;
    }

    return hitpoint.y;
}

int field::GetSquareNo(const Vector3& pos) const {
    if (m_divx <= 0 || m_divz <= 0) {
        return -1;
    }

    const float halfsizex = m_sizex / 2.0f;
    const float halfsizez = m_sizez / 2.0f;

    // 範囲チェック
    if (pos.x < -halfsizex || pos.x > halfsizex ||
        pos.z < -halfsizez || pos.z > halfsizez) {
        return -1;
    }

    // 左下原点の相対座標に変換
    const float relativex = pos.x + halfsizex;
    const float relativez = pos.z + halfsizez;

    const float mapchipwidth = std::fabs(m_sizex / m_divx);
    const float mapchipheight = std::fabs(m_sizez / m_divz);

    // インデックス算出（端にちょうど重なった時のために丸める）
    const int mapchipx = std::clamp(static_cast<int>(relativex / mapchipwidth), 0, m_divx - 1);
    const int mapchipz = std::clamp(static_cast<int>(relativez / mapchipheight), 0, m_divz - 1);

    return mapchipz * m_divx + mapchipx;
}

std::array<Vector3, 3> field::GetFaceVertex(int triangleindex) const {
    std::array<Vector3, 3> trianglevertices{};

    if (triangleindex < 0 || triangleindex >= trianglecount()) {
        return trianglevertices;
    }

    const std::vector<VERTEX_3D>& vertices = m_planemesh.GetVertices();
    const std::vector<uint32_t>& indices = m_planemesh.GetIndices();

    const size_t baseindex = static_cast<size_t>(triangleindex) * 3;

    trianglevertices[0] = vertices[indices[baseindex + 0]].Position;
    trianglevertices[1] = vertices[indices[baseindex + 1]].Position;
    trianglevertices[2] = vertices[indices[baseindex + 2]].Position;

    return trianglevertices;
}

void field::GetFaceVertex(int triangleindex, std::array<Vector3, 3>& trianglevertices) const {
    trianglevertices = GetFaceVertex(triangleindex);
}

std::array<Vector3, 3> field::GetCurrentTriangleVertex(const Vector3& pos) const {
    int triangleindex = -1;
    Vector3 hitpoint{};

    if (!raycastvertical(pos, triangleindex, hitpoint)) {
        return {};
    }

    return GetFaceVertex(triangleindex);
}

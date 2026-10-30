#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include "gameobject.h"
#include "../system/commontypes.h"
#include "../system/IScene.h"
#include "../system/CShader.h"
#include "../system/CMaterial.h"
#include "../system/CTexture.h"
#include "../system/CPlaneMesh.h"
#include "../system/CMeshRenderer.h"
#include "../system/CPlane.h"

// 地形（XZ平面の格子メッシュ）
//   ・1マス（四角形）は2枚の三角形でできている
//   ・三角形番号 = マス番号 * TRIANGLES_PER_SQUARE + (0 または 1)
//   ・高さ取得はワールド原点にフィールドが置かれている前提（m_srt は考慮しない）
class field : public gameobject {
public:
    // 1マスあたりの三角形数
    static constexpr int TRIANGLES_PER_SQUARE = 2;

    explicit field(SceneBase* scene) : gameobject(scene) {}

    void init() override;
    void update(uint64_t delta) override;
    void draw(uint64_t delta) override;
    void dispose() override;

    // メッシュを作る前（init前）に指定する
    void setsize(float sizex, float sizez) {
        m_sizex = sizex;
        m_sizez = sizez;
    }

    // メッシュを作る前（init前）に指定する
    void setdivide(int divx, int divz) {
        m_divx = divx;
        m_divz = divz;
    }

    // パーリンノイズで起伏をつける（平面方程式も作り直す）
    void makeundulationwithperlin(
        float minheight,   // 高さの最小値
        float maxheight,   // 高さの最大値
        float scale,       // ノイズのスケール
        float offsetx,     // オフセットX
        float offsetz      // オフセットZ
    );

    // 全三角形の平面方程式を作り直す（頂点を動かしたら必ず呼ぶ）
    void makeplaneequation();

    // 地面の高さを取得する（全三角形を総当たり：確認・比較用）
    float GetHeight(const Vector3& pos) const;

    // 地面の高さを取得する（マス番号で絞り込むので高速：通常はこちら）
    float GetHeight2(const Vector3& pos) const;

    // pos が何番目のマスかを返す（範囲外は -1）
    int GetSquareNo(const Vector3& pos) const;

    // 三角形番号から3頂点座標を取得する（番号が範囲外なら原点3つ）
    std::array<Vector3, 3> GetFaceVertex(int triangleindex) const;

    // 上の関数の出力引数版（従来の呼び出しとの互換用）
    void GetFaceVertex(int triangleindex, std::array<Vector3, 3>& trianglevertices) const;

    // pos が乗っている三角形の3頂点座標を取得する（範囲外は原点3つ）
    std::array<Vector3, 3> GetCurrentTriangleVertex(const Vector3& pos) const;

    float GetSizeX() const { return m_sizex; }
    float GetSizeZ() const { return m_sizez; }

private:
    // 真上へ伸ばしたレイで、pos が乗っている三角形と交点を求める
    bool raycastvertical(const Vector3& pos, int& triangleindex, Vector3& hitpoint) const;

    // 三角形番号の平面情報（平面方程式＋3頂点）を取得する
    bool getplaneinfo(int triangleindex, PLANEINFO& planeinfo) const;

    // メッシュ全体の三角形数
    int trianglecount() const {
        return static_cast<int>(m_planemesh.GetIndices().size() / 3);
    }

    // 頂点の高さを動かした後に法線を計算し直す
    static void recalculatenormals(
        std::vector<VERTEX_3D>& vertices,
        const std::vector<uint32_t>& indices
    );

    CShader m_Shader;
    CMaterial m_Material;
    CTexture m_Texture;

    CPlaneMesh m_planemesh;
    CMeshRenderer m_planerenderer;

    float m_sizex{ 1000.0f };
    float m_sizez{ 1000.0f };

    int32_t m_divx{ 10 };
    int32_t m_divz{ 10 };

    // 三角形番号順に並んだ平面方程式（makeplaneequation で作る）
    std::vector<CPlane> m_planeeq;
};

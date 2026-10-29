#pragma once
#include <cstdint>
#include <vector>
#include <numbers>
#include <cmath>
#include <wrl/client.h>
#include <d3d11.h>
#include <cassert>
#include <windows.h>
#include "../system/dx11helper.h"
#include "../system/renderer.h"

// 無名名前空間
namespace  {
	float ToRad(float degree)
	{
		return degree * std::numbers::pi_v<float> / 180.0f;
	}

	struct VERTEX {
		float x{};
		float y{};
		float z{};
	};
}
 
class cirlce {
private:
	float					m_radius{ 1 };	// 半径
	uint32_t				m_divide{20};	// 分割数
	std::vector<VERTEX>		m_vertices{};	// 頂点データ

	ComPtr<ID3D11Buffer> m_VertexBuffer{};				// 頂点バッファー	
	ComPtr<ID3D11VertexShader> m_VertexShader{};		// 頂点シェーダー
	ComPtr<ID3D11PixelShader> m_PixelShader{};			// ピクセルシェーダー
	ComPtr<ID3D11InputLayout> m_VertexLayout{};			// 頂点レイアウト

	// 頂点データ作成
	std::vector<VERTEX> createvertices(float radius) const {

		std::vector<VERTEX> vertices;

		vertices.clear();					// 頂点クリア

		float deltaangle;					// 角度の変化量
		deltaangle = 180.0f / m_divide;		// 角度の変化量を計算

		for (int i = 0; i < m_divide + 1; i++) {

			VERTEX v;

			// 上半分の頂点計算
			v.x = radius * cos(ToRad(deltaangle * i));
			v.y = radius * sin(ToRad(deltaangle * i));
			v.z = 0.0f;
			vertices.emplace_back(v);

			// 下半分の頂点計算
			v.x = radius * cos(ToRad(deltaangle * i));
			v.y = radius * sin(ToRad(-deltaangle * i));
			v.z = 0.0f;

			vertices.emplace_back(v);
		}

		return vertices;
	}
public:

	void init()
	{
		m_vertices.clear();
		m_vertices = createvertices(1.0f);

		{
			// 頂点バッファ生成
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;				// バッファ使用方法
			bd.ByteWidth = sizeof(VERTEX) * m_vertices.size();			// バッファの大きさ
			bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;	// 頂点バッファ
			bd.CPUAccessFlags = 0;						// CPUアクセス不要

			D3D11_SUBRESOURCE_DATA InitData{};
			InitData.pSysMem = m_vertices.data();		// バッファの初期値

			HRESULT hr = Renderer::GetDevice()->CreateBuffer(
				&bd, 
				&InitData, 
				m_VertexBuffer.GetAddressOf());		// バッファ生成

			assert(hr == S_OK);
		}

		{



		}



	}
	void draw() {
		unsigned int stride = sizeof(VERTEX);					// ストライドをセット（１頂点当たりのバイト数）
		unsigned  offset = 0;									// オフセット値をセット

		// 頂点バッファをデバイスコンテキストへセット
		Renderer::GetDeviceContext()->IASetVertexBuffers(
			0,													// スタートスロット
			1,													// 頂点バッファ個数
			m_VertexBuffer.GetAddressOf(),									// 頂点バッファの先頭アドレス
			&stride,											// ストライド
			&offset);											// オフセット

		Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

		// 頂点フォーマットをセット
		Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

		Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);		// 頂点シェーダーをセット
		Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);		// ピクセルシェーダーをセット

		Renderer::GetDeviceContext()->Draw(
			static_cast<uint32_t>(m_vertices.size()),
			0);									// インデックス数


	}
};
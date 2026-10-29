#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <array>
#include <cassert>
#include <typeinfo>
#include <utility>
#include "camera.h"

// 前方宣言
class gameobject;

// シーンインタフェース
class IScene {
public:
	IScene() = default;
	virtual ~IScene() = default;
	virtual void update(uint64_t delta) = 0;
	virtual void draw(uint64_t delta) = 0;
	virtual void init() = 0;
	virtual void dispose() = 0;
};

class SceneBase : public IScene {
public:
	static constexpr uint32_t LAYERMAX = 4;

protected:
	using ObjectList = std::vector<std::unique_ptr<gameobject>>;
	using LayerArray = std::array<ObjectList, LAYERMAX>;

	LayerArray m_gameobjects;			// 描画・更新対象
	LayerArray m_pendingAddObjects;		// 次フレームで追加するオブジェクト

	std::unique_ptr<Camera> m_camera;	// カメラ

public:
	Camera* getcamera() { return m_camera.get(); }

	template <typename T>
	T* addGameObject(int layer)
	{
		assert(layer >= 0 && layer < static_cast<int>(LAYERMAX));

		auto obj = std::make_unique<T>(this);
		obj->init();

		T* rawPtr = obj.get();
		m_pendingAddObjects[layer].push_back(std::move(obj));

		return rawPtr;
	}

	template <typename T>
	T* GetGameObject()
	{
		for (auto& objectList : m_gameobjects)
		{
			for (auto& object : objectList)		// object は unique_ptr<gameobject>&
			{
				if (typeid(*object) == typeid(T))	// RTTI で厳密に型一致を判定
				{
					return static_cast<T*>(object.get());
				}
			}
		}
		return nullptr;
	}

	template <typename T>
	std::vector<T*> GetGameObjects()
	{
		std::vector<T*> objects;

		for (auto& objectList : m_gameobjects)
		{
			for (auto& object : objectList)
			{
				if (typeid(*object) == typeid(T))
				{
					objects.push_back(static_cast<T*>(object.get()));
				}
			}
		}
		return objects;
	}
};
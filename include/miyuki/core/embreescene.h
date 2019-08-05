//
// Created by Shiina Miyuki on 2019/2/28.
//

#ifndef MIYUKI_EMBREESCENE_H
#define MIYUKI_EMBREESCENE_H

#include "miyuki.h"
#include "mesh.h"
#include <core/ray.h>
#include <core/intersection.hpp>
#include <core/accelerators/accelerator.h>
#include <embree3/rtcore.h>

namespace Miyuki {
	namespace Core {
		RTCDevice GetEmbreeDevice();

		class Scene;

		class EmbreeScene final: public Accelerator {
		public:
			MYK_CLASS(EmbreeScene);

			EmbreeScene();

			void commit();

			void addMesh(std::shared_ptr<Mesh> mesh, uint32_t id)override;

			bool intersect(const Ray& ray, Intersection* isct)override;

			RTCScene getRTCScene() {
				return scene;
			}
			~EmbreeScene();

			virtual Bound3f getWorldBound()const override;
		private:
			RTCScene scene;
		};
		MYK_IMPL(EmbreeScene, "Accelerator.Embree");
		MYK_REFL(EmbreeScene, (Accelerator), MYK_REFL_NIL);
	}
}

#endif //MIYUKI_EMBREESCENE_H

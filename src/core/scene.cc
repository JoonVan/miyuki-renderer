//
// Created by Shiina Miyuki on 2019/3/3.
//

#include "scene.h"
#include "cameras/camera.h"
#include "utils/thread.h"
#include "samplers/sampler.h"
#include "samplers/sobol.h"
#include "math/sampling.h"
#include "core/profile.h"
#include "lights/area.h"

namespace Miyuki {
    void Scene::useDefaultReadImageFunc() {
        readImageFunc = [&](std::vector<uint8_t> &pixelData) {
            for (int i = 0; i < film->width(); i++) {
                for (int j = 0; j < film->height(); j++) {
                    auto out = film->getPixel(i, j).toInt();
                    auto idx = i + film->width() * (film->height() - j - 1);
                    pixelData[4 * idx] = out.x();
                    pixelData[4 * idx + 1] = out.y();
                    pixelData[4 * idx + 2] = out.z();
                    pixelData[4 * idx + 3] = 255;
                }
            }
        };
    }

    Scene::Scene() : embreeScene(new EmbreeScene()) {
        setFilmDimension({1000, 1000});
        factory = std::make_unique<MaterialFactory>();
        updateFunc = [](Scene &x) {};
        processContinueFunc = [](Scene &x) { return true; };
        useDefaultReadImageFunc();
    }

    void Scene::setFilmDimension(const Point2i &dim) {
        film = std::make_unique<Film>(dim[0], dim[1]);
        camera = std::make_unique<PerspectiveCamera>(
                film->imageDimension(),
                parameterSet.findFloat("camera.fov", 80.0f) / 180.0f * PI,
                parameterSet.findFloat("camera.lensRadius", 0.0f),
                parameterSet.findFloat("camera.focalDistance", 0.0f));
        camera->rotateTo(parameterSet.findVec3f("camera.rotation", {}) / 180.0f * PI);
        camera->moveTo(parameterSet.findVec3f("camera.translation", {}));
        camera->preprocess();
    }

    void Scene::loadObjMesh(const std::string &filename) {
        if (meshes.find(filename) != meshes.end())
            return;
        auto mesh = std::make_shared<Mesh>(filename);
        meshes[filename] = mesh;
    }

    void Scene::loadObjMeshAndInstantiate(const std::string &name, const Transform &T) {
        loadObjMesh(name);
        instantiateMesh(name, T);
    }

    void Scene::instantiateMesh(const std::string &name, const Transform &T) {
        CHECK(meshes.find(name) != meshes.end());
        auto mesh = meshes[name]->instantiate(T);
        embreeScene->addMesh(mesh, instances.size());
        factory->applyMaterial(description["shapes"], description["materials"], *mesh);
        instances.emplace_back(mesh);
    }

    void Scene::computeLightDistribution() {
        lights.clear();
        for (const auto &mesh:instances) {
            for (auto &p:mesh->primitives) {
                if (p.material()->emission.albedo.max() > 0.0f) {
                    lights.emplace_back(std::make_shared<AreaLight>(&p));
                    p.setLight(lights.back().get());
                }
            }
        }
        Float *power = new Float[lights.size()];
        for (int i = 0; i < lights.size(); i++) {
            power[i] = lights[i]->power();
        }
        lightDistribution = std::make_unique<Distribution1D>(power, lights.size());
        lightPdfMap.clear();
        for (int i = 0; i < lights.size(); i++) {
            lightPdfMap[lights[i].get()] = lightDistribution->pdf(i);
        }
        delete[] power;
        Log::log("Important lights: {} Total power: {}\n", lights.size(), lightDistribution->funcInt);
    }

    void Scene::commit() {
        setFilmDimension(Point2i{parameterSet.findInt("render.width", 500),
                                 parameterSet.findInt("render.height", 500)});
        embreeScene->commit();
        camera->preprocess();
        Log::log("Film dimension: {}x{}\n", film->width(), film->height());
        Log::log("Output file: {}\n", parameterSet.findString("render.output", "scene.png"));
        computeLightDistribution();
        RTCBounds bounds;
        rtcGetSceneBounds(embreeScene->scene, &bounds);
        Float worldRadius = (Vec3f(bounds.lower_x, bounds.lower_y, bounds.lower_z)
                             - Vec3f(bounds.upper_x, bounds.upper_y, bounds.upper_z)).length() / 2;
        auto ambient = parameterSet.findVec3f("ambientLight", Vec3f());
        std::shared_ptr<IO::Image> envMap;
        auto envMapName = parameterSet.findString("envMap", "");
        if (!envMapName.empty()) {
            envMap = factory->loader.load(envMapName);
        }
        infiniteAreaLight = std::make_unique<InfiniteAreaLight>(worldRadius, Texture(ambient, envMap));
    }

    RenderContext Scene::getRenderContext(const Point2i &raster, MemoryArena *arena, Sampler *sampler) {
        sampler->start();
        Ray primary;
        Float weight;

        camera->generateRay(*sampler, raster, &primary, &weight);
        return RenderContext(raster, primary, camera.get(), arena, sampler, weight);
    }

    RenderContext Scene::getRenderContext(const Point2f &raster, MemoryArena *arena, Sampler *sampler) {
        Point2i r(clamp<int>(std::floor(raster.x() * film->width()), 0, film->width() - 1),
                  clamp<int>(std::floor(raster.y() * film->height()), 0, film->height() - 1));
        return getRenderContext(r, arena, sampler);
    }

    bool Scene::intersect(const RayDifferential &ray, Intersection *isct) {
        *isct = Intersection(ray);
        if (!isct->intersect(embreeScene->scene))
            return false;
        isct->uv = Point2f{isct->rayHit.hit.u, isct->rayHit.hit.v};
        isct->primitive = &instances[isct->geomId]->primitives[isct->primId];
        isct->wo = -1 * ray.d;
        auto p = isct->primitive;
        isct->Ns = p->Ns(isct->uv);
        isct->Ng = p->Ng();
        isct->ref = ray.o + isct->hitDistance() * ray.d;
        return true;
    }

    Light *Scene::chooseOneLight(Sampler *sampler, Float *pdf) {
        if (lights.empty()) {
            return nullptr;
        }
        auto light = lightDistribution->sampleInt(sampler->get1D());
        *pdf = lightDistribution->pdf(light);
        return lights[light].get();
    }

    void Scene::saveImage() {
        auto out = parameterSet.findString("render.output", "out.png");
        fmt::print("Image saved to {}\n", out);
        film->writePNG(out);
    }

    void Scene::readImage(std::vector<uint8_t> &pixelData) {
        if (pixelData.size() != film->width() * film->height() * 4)
            pixelData.resize(film->width() * film->height() * 4);
        readImageFunc(pixelData);
    }

    void Scene::update() {
        updateFunc(*this);
    }


}
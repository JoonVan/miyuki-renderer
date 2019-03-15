//
// Created by Shiina Miyuki on 2019/3/10.
//

#include "vertex.h"
#include <core/scene.h>
#include <integrators/integrator.h>

namespace Miyuki {
    namespace Bidir {

        SubPath
        RandomWalk(Vertex *vertices, Ray ray, Spectrum beta, Float pdf, Scene &scene,
                   RenderContext &ctx, int minDepth, int maxDepth,
                   TransportMode mode) {
            if (maxDepth == 0) {return { nullptr, 0 }; }
            Float pdfFwd = pdf, pdfRev = 0;
            int depth = 0;
            auto intersections = ctx.arena->alloc<Intersection>(size_t(maxDepth));
            auto events = ctx.arena->alloc<ScatteringEvent>(size_t(maxDepth));
            Float R = beta.max();
            if (R <= 0)
                return {nullptr, 0};
            while (true) {
                auto &vertex = vertices[depth];
                auto &prev = vertices[depth - 1];
                if (!scene.intersect(ray, &intersections[depth])) {
                    if (mode == TransportMode::radiance) {
                        // TODO: infinite light
                    }
                    break;
                }
                // TODO: medium
                Integrator::makeScatteringEvent(&events[depth], ctx, &intersections[depth]);
                auto &event = events[depth];
                vertex = CreateSurfaceVertex(&events[depth], pdfFwd, beta, prev);
                if (++depth >= maxDepth) {
                    break;
                }

                auto f = event.bsdf->sample(event);
                pdfFwd = event.pdf;

                if (f.isBlack() || pdfFwd <= 0)
                    break;
                beta *= f * Vec3f::absDot(event.Ns(), event.wiW) / event.pdf;
                beta *= CorrectShadingNormal(event, event.woW, event.wiW, mode);
                ray = event.spawnRay(event.wiW);
                std::swap(event.wi, event.wo);
                std::swap(event.wiW, event.woW);
                pdfRev = event.bsdf->pdf(event);
                std::swap(event.wi, event.wo);
                std::swap(event.wiW, event.woW);
                if (event.bsdfLobe.matchFlag(BSDFLobe::specular)) {
                    vertex.delta = true;
                    pdfRev = pdfFwd = 0;
                }

                prev.pdfRev = vertex.convertDensity(pdfRev, prev);
                if (depth >= minDepth) {
                    if (ctx.sampler->get1D() < beta.max() / R) {
                        beta *= R / beta.max();
                    } else {
                        break;
                    }
                }
            }
            return SubPath(vertices, depth);
        }

        Float Vertex::pdfLightOrigin(Scene &scene, const Vertex &v) const {
            // TODO: infinite lights
            // Current implementation doesn't allow rays to be emitted from infinite area lights
            if (isInfiniteLight())
                return 0;
            CHECK(light);
            auto w = (v.ref - ref).normalized();
            auto pdfChoice = scene.pdfLightChoice(light);
            Float pdfPos, pdfDir;
            light->pdfLe(Ray{ref, w}, Ng, &pdfPos, &pdfDir);
            return pdfPos * pdfChoice;
        }

    }
}
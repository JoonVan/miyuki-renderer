#include <core/materials/diffusematerial.h>
#include <core/bsdfs/bsdf.h>
#include <core/bsdfs/diffuse.hpp>
namespace Miyuki {
	namespace Core {
		class DiffuseBSDFImpl : public BSDFImpl {
			const Float roughness;
			const Spectrum R;
		public:
			DiffuseBSDFImpl(Float roughness, Spectrum R)
				:BSDFImpl(BSDFLobe(EReflection | EDiffuse)), R(R), roughness(roughness) {}

			virtual void sample(
				BSDFEvaluationContext& ctx,
				BSDFSample& sample
			)const override {
				if (roughness >= 1e-6f) {
					OrenNayarReflection(R, roughness).sample(sample);
				}
				else {
					LambertianReflection(R).sample(sample);
				}
				sample.lobe = getLobe();
				ctx.assignWi(sample.wi);
			}

			// evaluate bsdf according to wo, wi
			virtual Spectrum evaluate(
				const BSDFEvaluationContext& ctx
			)const override {
				auto& wo = ctx.wo();
				auto& wi = ctx.wi();
				if (roughness < 0.0f) {
					return LambertianReflection(R).evaluate(wo, wi);
				}
				else {
					return OrenNayarReflection(R, roughness).evaluate(wo, wi);
				}
			}

			// evaluate pdf according to wo, wi
			virtual Float evaluatePdf(
				const BSDFEvaluationContext& ctx
			)const override {
				auto& wo = ctx.wo();
				auto& wi = ctx.wi();
				if (roughness < 0.0f) {
					return LambertianReflection(R).evaluatePdf(wo, wi);
				}
				else {
					return OrenNayarReflection(R, roughness).evaluatePdf(wo, wi);
				}
			}
		};

		BSDFImpl* DiffuseMaterial::createBSDF(BSDFCreationContext& ctx)const {
			Float _roughness = Shader::evaluate(roughness, ctx.shadingPoint).toFloat();
			Spectrum R = Shader::evaluate(color, ctx.shadingPoint).toVec3f();
			auto bsdf = ctx.alloc<DiffuseBSDFImpl>(_roughness, R);
			return bsdf;
		}

	}
}
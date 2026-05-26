#pragma once

#include <SimpleMath.h>

namespace TEN::Effects::SnowDust
{
	using namespace DirectX::SimpleMath;

	// Subtle white spray emitted when an object freshly compresses pristine snow.
	// `worldRadius` is the footprint radius of the compressor (item collision radius,
	// foot brush size, ...). `intensity` is in [0..1] and reflects how much fresh
	// snow is being pushed down; pre-multiplied by the configured snow MaxDepth so
	// deeper snow always produces a more forceful puff. Particle count and size
	// scale with both inputs. Cheap; safe to call every tick per impact site.
	void SpawnSnowCompressionPuff(const Vector3& worldPos, int roomNumber,
								  float worldRadius, float intensity);

	// Heavy explosion burst: wet slush chunks fly out along ballistic arcs while a
	// dust cloud rises and dissipates. Also deforms the snow heightmap at `worldPos`
	// with the same radius (no-op if the snow system is inactive). Intended to be
	// called from script via Effects.SnowExplosion(...) on top of the normal
	// explosion FX.
	void SpawnSnowExplosionBurst(const Vector3& worldPos, int roomNumber, float worldRadius);
}

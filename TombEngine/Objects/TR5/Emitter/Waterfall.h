#pragma once

struct Particle;

namespace TEN::Effects::WaterfallEmitter
{
	void InitializeWaterfall(short itemNumber);
	void ControlWaterfall(short itemNumber);

	void SpawnWaterfallMist(const Vector3& pos, int roomNumber, float scalar, float size, const Color& color);
	bool HandleWaterfallParticle(Particle& particle);
}

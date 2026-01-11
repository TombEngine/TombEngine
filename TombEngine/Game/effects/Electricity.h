#pragma once

namespace TEN::Effects::Electricity
{
	constexpr auto ELECTRICITY_KNOTS_SIZE  = 6;
	constexpr auto ELECTRICITY_BUFFER_SIZE = 2048;

	enum class ElectricityFlags
	{
		Spline	 = (1 << 0),
		MoveEnd	 = (1 << 1),
		ThinOut	 = (1 << 2),
		ThinIn	 = (1 << 3),
		SparkEnd = (1 << 4)
	};

	// TODO: Make sense of this struct.
	struct Electricity
	{
		Vector3 pos1;
		Vector3 pos2;
		Vector3 pos3;
		Vector3 pos4;
		std::array<Vector3, 3> interpolation = {};

		byte r;
		byte g;
		byte b;

		float life;
		float sLife;
		float amplitude;
		float sAmplitude;
		float width;
		unsigned int segments;

		int segmentSize;
		int direction;
		int rotation;
		int type;
		int flags;

		Vector3 PrevPos1 = Vector3::Zero;
		Vector3 PrevPos2 = Vector3::Zero;
		Vector3 PrevPos3 = Vector3::Zero;
		Vector3 PrevPos4 = Vector3::Zero;
		byte	PrevR	 = 0;
		byte	PrevG	 = 0;
		byte	PrevB	 = 0;
		float	PrevLife = 0.0f;

		void StoreInterpolationData()
		{
			PrevPos1 = pos1;
			PrevPos2 = pos2;
			PrevPos3 = pos3;
			PrevPos4 = pos4;
			PrevR = r;
			PrevG = g;
			PrevB = b;
			PrevLife = life;
		}
	};

	struct HelicalLaser
	{
		unsigned int NumSegments = 0;

		Vector3 Origin		  = Vector3::Zero;
		Vector3 Target		  = Vector3::Zero;
		short	Orientation2D = 0;
		Vector3 LightPosition = Vector3::Zero; // TODO: Use light cone instead?
		Vector4 Color		  = Vector4::Zero;

		float Life		= 0.0f;
		float Radius	= 0.0f;
		float Length	= 0.0f;
		float LengthEnd = 0.0f;
		float Opacity	= 0.0f;
		short Rotation	= 0;

		Vector3 PrevOrigin		  = Vector3::Zero;
		Vector3 PrevTarget		  = Vector3::Zero;
		short	PrevOrientation2D = 0;
		Vector4 PrevColor		  = Vector4::Zero;
		float	PrevLife		  = 0.0f;
		float	PrevRadius		  = 0.0f;
		float	PrevLength		  = 0.0f;
		float	PrevOpacity		  = 0.0f;

		void StoreInterpolationData()
		{
			PrevOrigin = Origin;
			PrevTarget = Target;
			PrevOrientation2D = Orientation2D;
			PrevColor = Color;
			PrevLife = Life;
			PrevRadius = Radius;
			PrevLength = Length;
			PrevOpacity = Opacity;
		}
	};

	extern std::vector<Electricity>	 ElectricityArcs;
	extern std::vector<HelicalLaser> HelicalLasers;

	extern std::array<Vector3, ELECTRICITY_KNOTS_SIZE>	ElectricityKnots;
	extern std::array<Vector3, ELECTRICITY_BUFFER_SIZE> ElectricityBuffer;

	void SpawnElectricity(const Vector3& origin, const Vector3& target, float amplitude, byte r, byte g, byte b, float life, int flags, float width, unsigned int numSegments);
	void SpawnElectricEffect(const ItemInfo& item, int jointNumber, const Vector3i& offset, const float spawnRadius, float beamOriginRadius, float beamTargetRadius, int frequency, const Vector3& pos);
	void SpawnElectricityGlow(const Vector3& pos, float scale, byte r, byte g, byte b);
	void SpawnHelicalLaser(const Vector3& origin, const Vector3& target);
	void UpdateElectricityArcs();
	void UpdateHelicalLasers();

	void CalculateElectricitySpline(const Electricity& arc, const std::array<Vector3, ELECTRICITY_KNOTS_SIZE>& knots, std::array<Vector3, ELECTRICITY_BUFFER_SIZE>& buffer);
	void CalculateHelixSpline(const HelicalLaser& laser, std::array<Vector3, ELECTRICITY_KNOTS_SIZE>& knots, std::array<Vector3, ELECTRICITY_BUFFER_SIZE>& buffer);
}

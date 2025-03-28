#pragma once

#include "Math/Math.h"

using namespace TEN::Math;

enum GAME_OBJECT_ID : short;
enum class ReverbType;
class FloorInfo;
class GameBoundingBox;
struct BUCKET;
struct TriggerVolume;

constexpr auto MAX_FLIPMAP	= 256;
constexpr auto NUM_ROOMS	= 1024;
constexpr auto OUTSIDE_Z	= 64;
constexpr auto OUTSIDE_SIZE = 1024;

extern bool FlipStatus;
extern bool FlipStats[MAX_FLIPMAP];
extern int  FlipMap[MAX_FLIPMAP];

enum RoomEnvFlags
{
	ENV_FLAG_WATER			 = 1 << 0,
	ENV_FLAG_SWAMP			 = 1 << 2,
	ENV_FLAG_SKYBOX			 = 1 << 3,
	ENV_FLAG_DYNAMIC_LIT	 = 1 << 4,
	ENV_FLAG_WIND			 = 1 << 5,
	ENV_FLAG_NOT_NEAR_SKYBOX = 1 << 6,
	ENV_FLAG_NO_LENSFLARE	 = 1 << 7,
	ENV_FLAG_MIST			 = 1 << 8,
	ENV_FLAG_CAUSTICS		 = 1 << 9,
	ENV_FLAG_UNKNOWN3		 = 1 << 10,
	ENV_FLAG_DAMAGE			 = 1 << 11,
	ENV_FLAG_COLD			 = 1 << 12
};

enum StaticMeshFlags : short
{
	SM_VISIBLE	 = 1 << 0,
	SM_SOLID	 = 1 << 1,
	SM_COLLISION = 1 << 2
};

struct ROOM_VERTEX
{
	Vector3 position;
	Vector3 normal;
	Vector2 textureCoordinates;
	Vector3 color;
	int		effects;
	int		index;
};

struct MESH_INFO
{
	Pose pos;
	int roomNumber;
	float scale;
	short staticNumber;
	short flags;
	Vector4 color;
	short HitPoints;
	std::string Name;
	bool Dirty;
	
	BoundingOrientedBox GetObb() const;
	BoundingOrientedBox GetVisibilityObb() const;
};

struct RoomLightData
{
	int x, y, z;       // Position of light, in world coordinates
	float r, g, b;       // Colour of the light
	float intensity;
	float in;            // Cosine of the IN value for light / size of IN value
	float out;           // Cosine of the OUT value for light / size of OUT value
	float length;         // Range of light
	float cutoff;         // Range of light
	float dx, dy, dz;    // Direction - used only by sun and spot lights
	byte type;
	bool castShadows;
};

struct PortalData
{
private:
	static constexpr auto VERTEX_COUNT = 4;

public:
	int			  RoomNumber	= 0;
	CollisionMesh CollisionMesh = {};
	Vector3		  Normal		= Vector3::Zero;

	std::array<Vector3, VERTEX_COUNT> Vertices = {};
};

class RoomObjectHandler
{
private:
	// Constants

	static constexpr auto AABB_BOUNDARY = BLOCK(0.1f);

	// Fields

	Bvh _tree = Bvh();

public:
	// Constructors

	RoomObjectHandler() = default;

	// Getters

	std::vector<int> GetIds() const;
	std::vector<int> GetBoundedIds(const Ray& ray, float dist) const;
	std::vector<int> GetBoundedIds(const BoundingSphere& sphere) const;

	// Utilities

	void Insert(int id, const BoundingBox& aabb);
	void Move(int id, const BoundingBox& aabb);
	void Remove(int id);

	// Debug

	void DrawDebug() const;
};

// TODO: Make class?
struct RoomData
{
	int						 RoomNumber = 0;
	std::string				 Name		= {};
	std::vector<std::string> Tags		= {};

	Vector3i	Position	 = Vector3i::Zero;
	BoundingBox Aabb		 = BoundingBox();
	int			BottomHeight = 0; // Deprecated. Can derive from AABB instead.
	int			TopHeight	 = 0; // Deprecated. Can derive from AABB instead.
	int			XSize		 = 0;
	int			ZSize		 = 0;

	Vector3 ambient;
	int flags;
	int meshEffect;
	ReverbType reverbType;
	int flippedRoom;
	int flipNumber;
	short itemNumber;
	short fxNumber;
	bool boundActive;

	std::vector<int> NeighborRoomNumbers = {};

	//RoomObjectHandler Moveables = RoomObjectHandler(); // TODO: Refactor linked list of items in room to use a BVH instead.
	//RoomObjectHandler Statics	= RoomObjectHandler(); // TODO: Refactor to use BVH.
	std::vector<MESH_INFO> mesh = {}; // Statics

	CollisionMesh			   CollisionMesh  = TEN::Physics::CollisionMesh();
	RoomObjectHandler		   Bridges		  = RoomObjectHandler();
	std::vector<PortalData>	   Portals		  = {};
	std::vector<TriggerVolume> TriggerVolumes = {};
	std::vector<FloorInfo>	   Sectors		  = {};

	std::vector<RoomLightData> lights	 = {};
	std::vector<Vector3>	   positions = {};
	std::vector<Vector3>	   normals	 = {};
	std::vector<Vector3>	   colors	 = {};
	std::vector<Vector3>	   effects	 = {};
	std::vector<BUCKET>		   buckets	 = {};

	bool Active() const;
	void GenerateCollisionMesh();

private:
	void CollectSectorCollisionMeshTriangles(CollisionMeshDesc& desc,
											 const FloorInfo& sector,
											 const FloorInfo& sectorNorth, const FloorInfo& sectorSouth,
											 const FloorInfo& sectorEast, const FloorInfo& sectorWest);
};

void DoFlipMap(int group);
void ResetRoomData();
bool IsObjectInRoom(int roomNumber, GAME_OBJECT_ID objectID);
bool IsPointInRoom(const Vector3i& pos, int roomNumber);
int FindRoomNumber(const Vector3i& pos, int startRoomNumber = NO_VALUE, bool onlyNeighbors = false);
Vector3i GetRoomCenter(int roomNumber);
int IsRoomOutside(int x, int y, int z);
void InitializeNeighborRoomList();

GameBoundingBox& GetBoundsAccurate(const MESH_INFO& mesh, bool getVisibilityBox);
std::vector<int> GetNeighborRoomNumbers(int roomNumber, unsigned int searchDepth);

namespace TEN::Collision::Room
{
	FloorInfo* GetSector(RoomData* room, int x, int z);
}

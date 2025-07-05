#pragma once

#include "Game/StaticMesh.h"
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
	ENV_FLAG_WATER			 = (1 << 0),
	ENV_FLAG_SWAMP			 = (1 << 2),
	ENV_FLAG_SKYBOX			 = (1 << 3),
	ENV_FLAG_DYNAMIC_LIT	 = (1 << 4),
	ENV_FLAG_WIND			 = (1 << 5),
	ENV_FLAG_NOT_NEAR_SKYBOX = (1 << 6),
	ENV_FLAG_NO_LENSFLARE	 = (1 << 7),
	ENV_FLAG_MIST			 = (1 << 8),
	ENV_FLAG_CAUSTICS		 = (1 << 9),
	ENV_FLAG_UNKNOWN3		 = (1 << 10),
	ENV_FLAG_DAMAGE			 = (1 << 11),
	ENV_FLAG_COLD			 = (1 << 12)
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

struct ROOM_DOOR
{
	short room;
	Vector3 normal;
	Vector3 vertices[4];
};

struct ROOM_LIGHT
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

struct ROOM_INFO
{
	int						 RoomNumber = 0;
	std::string				 Name		= {};
	std::vector<std::string> Tags		= {};

	Vector3i Position	  = Vector3i::Zero;
	int		 BottomHeight = 0;
	int		 TopHeight	  = 0;
	int		 XSize		  = 0;
	int		 ZSize		  = 0;

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

	std::vector<FloorInfo>	   Sectors		  = {};
	std::vector<ROOM_LIGHT>	   lights		  = {};
	std::vector<StaticMesh>	   mesh			  = {}; // Statics
	std::vector<TriggerVolume> TriggerVolumes = {};

	std::vector<Vector3>   positions = {};
	std::vector<Vector3>   normals	 = {};
	std::vector<Vector3>   colors	 = {};
	std::vector<Vector3>   effects	 = {};
	std::vector<BUCKET>	   buckets	 = {};
	std::vector<ROOM_DOOR> doors	 = {};

	bool Active() const;
};

void DoFlipMap(int group);
void ResetRoomData();
bool IsObjectInRoom(int roomNumber, GAME_OBJECT_ID objectID);
bool IsPointInRoom(const Vector3i& pos, int roomNumber);
int FindRoomNumber(const Vector3i& pos, int startRoomNumber = NO_VALUE, bool onlyNeighbors = false);
Vector3i GetRoomCenter(int roomNumber);
int IsRoomOutside(int x, int y, int z);
void InitializeNeighborRoomList();

std::vector<int> GetNeighborRoomNumbers(int roomNumber, unsigned int searchDepth, std::vector<int>& visitedRoomNumbers = std::vector<int>{});

namespace TEN::Collision::Room
{
	FloorInfo* GetSector(ROOM_INFO* room, int x, int z);
}

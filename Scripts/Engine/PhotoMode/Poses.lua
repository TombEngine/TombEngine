-- ldignore
local PhotoModeData = require("PhotoModeData")

local Poses =
{
    { name = "Default", objID = TEN.Objects.ObjID.LARA,            animNumber = 0,  frameNumber = 0 },
    { name = "0",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 0,  frameNumber = 0 },
    { name = "1",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 1,  frameNumber = 0 },
    { name = "2",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 2,  frameNumber = 0 },
    { name = "3",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 3,  frameNumber = 0 },
    { name = "4",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 4,  frameNumber = 0 },
    { name = "5",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 5,  frameNumber = 0 },
    { name = "6",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 6,  frameNumber = 0 },
    { name = "7",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 7,  frameNumber = 0 },
    { name = "8",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 8,  frameNumber = 0 },
    { name = "9",       objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 9,  frameNumber = 0 },
    { name = "10",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 10, frameNumber = 0 },
    { name = "11",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 11, frameNumber = 0 },
    { name = "12",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 12, frameNumber = 0 },
    { name = "13",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 13, frameNumber = 0 },
    { name = "14",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 14, frameNumber = 0 },
    { name = "15",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 15, frameNumber = 0 },
    { name = "16",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 16, frameNumber = 0 },
    { name = "17",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 17, frameNumber = 0 },
    { name = "18",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 18, frameNumber = 0 },
    { name = "19",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 19, frameNumber = 0 },
    { name = "20",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 20, frameNumber = 0 },
    { name = "21",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 21, frameNumber = 0 },
    { name = "22",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 22, frameNumber = 0 },
    { name = "23",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 23, frameNumber = 0 },
    { name = "24",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 24, frameNumber = 0 },
    { name = "25",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 25, frameNumber = 0 },
    { name = "26",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 26, frameNumber = 0 },
    { name = "27",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 27, frameNumber = 0 },
    { name = "28",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 28, frameNumber = 0 },
    { name = "29",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 29, frameNumber = 0 },
    { name = "30",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 30, frameNumber = 0 },
    { name = "31",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 31, frameNumber = 0 },
    { name = "32",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 32, frameNumber = 0 },
    { name = "33",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 33, frameNumber = 0 },
    { name = "34",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 34, frameNumber = 0 },
    { name = "35",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 35, frameNumber = 0 },
    { name = "36",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 36, frameNumber = 0 },
    { name = "37",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 37, frameNumber = 0 },
    { name = "38",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 38, frameNumber = 0 },
    { name = "39",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 39, frameNumber = 0 },
    { name = "40",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 40, frameNumber = 0 },
    { name = "41",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 41, frameNumber = 0 },
    { name = "42",      objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, animNumber = 42, frameNumber = 0 },
}

for _, pose in ipairs(PhotoModeData.Poses) do
    table.insert(Poses, pose)
end

return Poses
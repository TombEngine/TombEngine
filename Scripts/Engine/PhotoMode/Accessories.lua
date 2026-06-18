-- ldignore
local PhotoModeData = require("PhotoModeData")

local Accessories =
{
    { name = "None",        objID = nil,                               meshIndices = {}   },
    { name = "Sunglasses",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {14} },
}

for _, accessory in ipairs(PhotoModeData.Accessories) do
    table.insert(Accessories, accessory)
end

return Accessories

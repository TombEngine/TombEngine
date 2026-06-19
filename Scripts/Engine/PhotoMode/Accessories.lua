-- ldignore
local PhotoModeSetup = require("PhotoModeSetup")

local Accessories =
{
    { name = "None",        objID = nil,                               meshIndices = {}   },
    { name = "Sunglasses",  objID = TEN.Objects.ObjID.PHOTOMODE_ANIMS, meshIndices = {14} },
}

for _, accessory in ipairs(PhotoModeSetup.Accessories) do
    table.insert(Accessories, accessory)
end

return Accessories

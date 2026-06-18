-- ldignore
local PhotoModeData = require("PhotoModeData")

local Expressions =
{
    { name = "Default", objID = nil, meshIndices = {} },
    { name = "Scream", objID = TEN.Objects.ObjID.LARA_SCREAM, meshIndices = {14} }
}

for _, expression in ipairs(PhotoModeData.Expressions) do
    table.insert(Expressions, expression)
end

return Expressions
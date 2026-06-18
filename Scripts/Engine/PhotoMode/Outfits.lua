-- ldignore
local PhotoModeData = require("PhotoModeData")

local Outfits =
{
    -- Index 1: Default — restores whatever state was active on photo mode entry.
  {
    name = "Default"
  }
}

for _, outfit in ipairs(PhotoModeData.Outfits) do
    table.insert(Outfits, outfit)
end

return Outfits
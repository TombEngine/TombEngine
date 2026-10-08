-- Renderer benchmark scenario: keeps the player alive and slowly turns her so the
-- chase camera sweeps the whole scene around the start position.

local frame = 0

LevelFuncs.OnStart = function() end
LevelFuncs.OnLoad = function() end
LevelFuncs.OnSave = function() end
LevelFuncs.OnEnd = function() end

LevelFuncs.OnLoop = function()
	frame = frame + 1
	Lara:SetHP(1000)
	Lara:SetRotation(Rotation.new(0, (frame * 0.75) % 360, 0))

	-- Particles exercise the sprite paths: additive (instanced) and alpha blended (sorted).
	local pos = Lara:GetPosition()
	for i = 0, 3 do
		local angle = (frame * 7 + i * 90) % 360
		local offset = Vec3(math.sin(math.rad(angle)) * 768, -512 - i * 128, math.cos(math.rad(angle)) * 768)
		local blend = (i % 2 == 0) and TEN.Effects.BlendID.ADDITIVE or TEN.Effects.BlendID.ALPHA_BLEND
		TEN.Effects.EmitParticle(pos + offset, Vec3(0, -4, 0), 0, 2, 30, TEN.Color(255, 160, 64), TEN.Color(64, 64, 255), blend, 64, 16, 3)
	end
end

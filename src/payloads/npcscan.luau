local hs = game:GetService("HttpService")
local players = game:GetService("Players")
local lp = players.LocalPlayer

local ALIVE_ONLY = __ALIVE_ONLY__
local SORT_BY_DISTANCE = __SORT_BY_DISTANCE__
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	npcs = {},
	total_found = 0,
	truncated = false,
	player_pos = nil,
}

local root_pos = nil
if lp and lp.Character then
	local hrp = lp.Character:FindFirstChild("HumanoidRootPart")
	if hrp and hrp:IsA("BasePart") then
		root_pos = hrp.Position
		out.player_pos = {
			x = math.floor(root_pos.X * 100 + 0.5) / 100,
			y = math.floor(root_pos.Y * 100 + 0.5) / 100,
			z = math.floor(root_pos.Z * 100 + 0.5) / 100,
		}
	end
end

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local hums = {}
local ok_q, qres = pcall(function() return workspace:QueryDescendants("Humanoid") end)
if ok_q and type(qres) == "table" then
	hums = qres
else
	local ok_d, desc = pcall(function() return workspace:GetDescendants() end)
	if ok_d and type(desc) == "table" then
		for i, inst in ipairs(desc) do
			if i % 4000 == 0 then task.wait() end
			if inst:IsA("Humanoid") then
				table.insert(hums, inst)
			end
		end
	end
end

local candidates = {}
for _, hum in ipairs(hums) do
	local model = hum.Parent
	if model and model:IsA("Model") then
		local ok_p, plr = pcall(function() return players:GetPlayerFromCharacter(model) end)
		if not (ok_p and plr ~= nil) then
			local hp = hum.Health
			local max_hp = hum.MaxHealth
			if (not ALIVE_ONLY) or (hp > 0) then
				local pos = nil
				local root = model:FindFirstChild("HumanoidRootPart") or model.PrimaryPart or model:FindFirstChildWhichIsA("BasePart")
				if root and root:IsA("BasePart") then
					pos = root.Position
				else
					local ok_pv, pv = pcall(function() return model:GetPivot().Position end)
					if ok_pv then pos = pv end
				end
				local dist = (pos and root_pos) and (math.floor((pos - root_pos).Magnitude * 100 + 0.5) / 100) or nil
				table.insert(candidates, {
					model = model,
					hum = hum,
					hp = hp,
					max_hp = max_hp,
					pos = pos,
					distance = dist,
				})
			end
		end
	end
end

out.total_found = #candidates

if SORT_BY_DISTANCE and root_pos then
	table.sort(candidates, function(a, b)
		return (a.distance or math.huge) < (b.distance or math.huge)
	end)
end

local limit = math.min(#candidates, MAX_ITEMS)
if #candidates > MAX_ITEMS then
	out.truncated = true
end

for i = 1, limit do
	local c = candidates[i]
	local m = c.model
	local has_prompt = m:FindFirstChildWhichIsA("ProximityPrompt", true) ~= nil
	local has_click = m:FindFirstChildWhichIsA("ClickDetector", true) ~= nil
	local entry = {
		name = m.Name,
		path = full_path(m),
		health = math.floor(c.hp * 100 + 0.5) / 100,
		max_health = math.floor(c.max_hp * 100 + 0.5) / 100,
		has_prompt = has_prompt,
		has_click = has_click,
		distance = c.distance,
	}
	if c.pos then
		entry.pos = {
			x = math.floor(c.pos.X * 100 + 0.5) / 100,
			y = math.floor(c.pos.Y * 100 + 0.5) / 100,
			z = math.floor(c.pos.Z * 100 + 0.5) / 100,
		}
	end
	table.insert(out.npcs, entry)
end

return hs:JSONEncode(out)

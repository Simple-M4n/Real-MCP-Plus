local hs = game:GetService("HttpService")
local cs = game:GetService("CollectionService")
local lp = game:GetService("Players").LocalPlayer

local MAX_ITEMS = __MAX_ITEMS__

local zone_keywords = {
	"zone", "area", "region", "safe", "safezone", "portal", "teleport",
	"warp", "checkpoint", "stage", "spawn", "farm", "boss", "arena",
	"lobby", "shop", "door", "gate", "island", "world", "dungeon"
}

local out = {
	spawns = {},
	zones = {},
	tags = {},
	total_found = 0,
	truncated = false,
}

local root_pos = nil
if lp and lp.Character then
	local hrp = lp.Character:FindFirstChild("HumanoidRootPart")
	if hrp and hrp:IsA("BasePart") then
		root_pos = hrp.Position
	end
end

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function match_kw(name)
	local low = string.lower(tostring(name))
	for _, kw in ipairs(zone_keywords) do
		if string.find(low, kw, 1, true) then
			return kw
		end
	end
	return nil
end

local function pos_and_size(inst)
	local pos, size = nil, nil
	if inst:IsA("BasePart") then
		pos = inst.Position
		size = inst.Size
	elseif inst:IsA("Model") then
		local ok, cf, sz = pcall(function() return inst:GetBoundingBox() end)
		if ok then
			pos = cf.Position
			size = sz
		else
			local ok_p, pv = pcall(function() return inst:GetPivot().Position end)
			if ok_p then pos = pv end
		end
	end
	local p_tbl = pos and {
		x = math.floor(pos.X * 100 + 0.5) / 100,
		y = math.floor(pos.Y * 100 + 0.5) / 100,
		z = math.floor(pos.Z * 100 + 0.5) / 100,
	} or nil
	local s_tbl = size and {
		x = math.floor(size.X * 100 + 0.5) / 100,
		y = math.floor(size.Y * 100 + 0.5) / 100,
		z = math.floor(size.Z * 100 + 0.5) / 100,
	} or nil
	local dist = (pos and root_pos) and (math.floor((pos - root_pos).Magnitude * 100 + 0.5) / 100) or nil
	return p_tbl, s_tbl, dist
end

local ok_sp, spawns = pcall(function() return workspace:QueryDescendants("SpawnLocation") end)
if ok_sp and type(spawns) == "table" then
	for _, sp in ipairs(spawns) do
		local p, s, d = pos_and_size(sp)
		table.insert(out.spawns, {
			name = sp.Name,
			path = full_path(sp),
			pos = p,
			size = s,
			distance = d,
			team_color = tostring(sp.TeamColor),
		})
	end
end

local ok_tags, all_tags = pcall(function() return cs:GetAllTags() end)
if ok_tags and type(all_tags) == "table" then
	for _, tag in ipairs(all_tags) do
		if #out.tags < 100 then
			table.insert(out.tags, tag)
		end
	end
end

local seen = {}
local function scan_level(container, depth)
	if not container or depth > 3 then return end
	local ok, children = pcall(function() return container:GetChildren() end)
	if not ok or type(children) ~= "table" then return end
	for _, child in ipairs(children) do
		local kw = match_kw(child.Name)
		if kw and (child:IsA("BasePart") or child:IsA("Model") or child:IsA("Folder")) then
			local path = full_path(child)
			if not seen[path] then
				seen[path] = true
				out.total_found += 1
				if #out.zones < MAX_ITEMS then
					local p, s, d = pos_and_size(child)
					table.insert(out.zones, {
						name = child.Name,
						class = child.ClassName,
						keyword = kw,
						path = path,
						pos = p,
						size = s,
						distance = d,
					})
				else
					out.truncated = true
				end
			end
		end
		if child:IsA("Folder") or child:IsA("Model") then
			scan_level(child, depth + 1)
		end
	end
end

scan_level(workspace, 1)

return hs:JSONEncode(out)

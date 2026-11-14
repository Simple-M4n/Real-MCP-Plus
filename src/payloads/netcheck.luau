local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local TARGET_PATH = __TARGET_PATH__
local UNANCHORED_ONLY = __UNANCHORED_ONLY__
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	has_isnetworkowner = typeof(isnetworkowner) == "function",
	items = {},
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

local function resolve_path(path)
	if path == "" or path == "workspace" or path == "Workspace" then
		return workspace
	end
	local cur = game
	for seg in string.gmatch(path, "[^%.]+") do
		if seg ~= "game" then
			local ok, child = pcall(function() return cur:FindFirstChild(seg) end)
			if not ok or not child then return nil end
			cur = child
		end
	end
	return cur
end

local root = resolve_path(TARGET_PATH)
if not root then
	return hs:JSONEncode({ error = "target_not_found: " .. TARGET_PATH })
end

local function inspect_part(part)
	if not part:IsA("BasePart") then return end
	if UNANCHORED_ONLY and part.Anchored then return end
	out.total_found += 1
	if #out.items >= MAX_ITEMS then
		out.truncated = true
		return
	end

	local net_owner = nil
	if out.has_isnetworkowner then
		local ok, own = pcall(isnetworkowner, part)
		if ok then net_owner = own end
	end

	local vel = part.AssemblyLinearVelocity
	local dist = root_pos and (math.floor((part.Position - root_pos).Magnitude * 100 + 0.5) / 100) or nil
	table.insert(out.items, {
		name = part.Name,
		class = part.ClassName,
		path = full_path(part),
		anchored = part.Anchored,
		can_collide = part.CanCollide,
		massless = part.Massless,
		network_owner = net_owner,
		speed = math.floor(vel.Magnitude * 100 + 0.5) / 100,
		distance = dist,
	})
end

if root:IsA("BasePart") then
	inspect_part(root)
else
	local ok, list = pcall(function() return root:GetDescendants() end)
	if ok and type(list) == "table" then
		for i, inst in ipairs(list) do
			if i % 4000 == 0 then task.wait() end
			inspect_part(inst)
			if #out.items >= MAX_ITEMS then break end
		end
	end
end

return hs:JSONEncode(out)

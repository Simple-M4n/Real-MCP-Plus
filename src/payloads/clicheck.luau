local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local MAX_ITEMS = __MAX_ITEMS__

local keywords = {
	"cooldown", "debounce", "delay", "range", "distance", "dist", "maxdist",
	"damage", "dmg", "speed", "walkspeed", "jumppower", "vip", "admin",
	"owner", "whitelist", "unlocked", "canbuy", "cost", "price", "cash",
	"money", "coins", "gems", "stamina", "energy", "ammo", "reload", "firerate"
}

local out = {
	trusted_values = {},
	trusted_attributes = {},
	total_found = 0,
	truncated = false,
}

local function match_kw(name)
	local low = string.lower(tostring(name))
	for _, kw in ipairs(keywords) do
		if string.find(low, kw, 1, true) then
			return kw
		end
	end
	return nil
end

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function scan_root(root, label)
	if not root then return end
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for i, inst in ipairs(list) do
		if i % 5000 == 0 then task.wait() end
		if inst:IsA("ValueBase") then
			local kw = match_kw(inst.Name)
			if kw then
				out.total_found += 1
				if #out.trusted_values < MAX_ITEMS then
					local ok_v, val = pcall(function() return inst.Value end)
					table.insert(out.trusted_values, {
						origin = label,
						class = inst.ClassName,
						name = inst.Name,
						value = ok_v and tostring(val) or nil,
						keyword = kw,
						path = full_path(inst),
					})
				else
					out.truncated = true
				end
			end
		end

		local ok_a, attrs = pcall(function() return inst:GetAttributes() end)
		if ok_a and type(attrs) == "table" then
			for k, v in pairs(attrs) do
				local kw = match_kw(k)
				if kw then
					out.total_found += 1
					if #out.trusted_attributes < MAX_ITEMS then
						table.insert(out.trusted_attributes, {
							origin = label,
							instance = full_path(inst),
							attribute = tostring(k),
							value = tostring(v),
							keyword = kw,
						})
					else
						out.truncated = true
					end
				end
			end
		end
	end
end

if lp then
	scan_root(lp, "LocalPlayer")
	if lp.Character then
		scan_root(lp.Character, "Character")
	end
end
scan_root(game:GetService("ReplicatedStorage"), "ReplicatedStorage")
scan_root(game:GetService("StarterPack"), "StarterPack")

return hs:JSONEncode(out)

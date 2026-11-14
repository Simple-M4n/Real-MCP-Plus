local hs = game:GetService("HttpService")

local FILTER = string.lower(__FILTER__)
local REQUIRE_MODULES = __REQUIRE_MODULES__
local MAX_ITEMS = __MAX_ITEMS__

local config_keywords = {
	"config", "setting", "settings", "constant", "constants", "stat", "stats",
	"item", "items", "weapon", "weapons", "gun", "shop", "store", "drop",
	"rate", "tier", "admin", "rank", "level", "quest", "pet", "unit", "data",
	"table", "catalog", "price", "egg", "crate"
}

local out = {
	modules = {},
	total_matched = 0,
	truncated = false,
}

local function match_kw(name)
	local low = string.lower(tostring(name))
	for _, kw in ipairs(config_keywords) do
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

local function preview_val(v, depth)
	local t = typeof(v)
	if t == "string" then
		return string.sub(v, 1, 120)
	elseif t == "number" or t == "boolean" then
		return v
	elseif t == "Instance" then
		return full_path(v)
	elseif t == "table" then
		if depth >= 2 then
			return "<table:" .. tostring(v) .. ">"
		end
		local res = {}
		local n = 0
		for k, val in pairs(v) do
			n += 1
			if n > 12 then
				res["..."] = "truncated"
				break
			end
			res[tostring(k)] = preview_val(val, depth + 1)
		end
		return res
	else
		return tostring(v)
	end
end

local seen = {}
local function scan_root(root, origin, is_nil)
	if not root then return end
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for i, inst in ipairs(list) do
		if i % 4000 == 0 then task.wait() end
		if inst:IsA("ModuleScript") then
			local path = full_path(inst)
			if not seen[path] then
				local low = string.lower(inst.Name .. " " .. path)
				local matched = (#FILTER > 0 and string.find(low, FILTER, 1, true) ~= nil)
					or (#FILTER == 0 and match_kw(inst.Name) ~= nil)
				if matched then
					seen[path] = true
					out.total_matched += 1
					if #out.modules < MAX_ITEMS then
						local entry = {
							origin = origin,
							is_nil = is_nil,
							name = inst.Name,
							path = path,
						}
						if REQUIRE_MODULES then
							local ok_req, data = pcall(require, inst)
							if ok_req then
								entry.require_ok = true
								entry.value_type = typeof(data)
								entry.preview = preview_val(data, 0)
							else
								entry.require_ok = false
								entry.require_error = tostring(data)
							end
						end
						table.insert(out.modules, entry)
					else
						out.truncated = true
					end
				end
			end
		end
	end
end

for _, svc_name in ipairs({"ReplicatedStorage", "ReplicatedFirst", "StarterPlayer", "StarterGui", "StarterPack", "Workspace"}) do
	pcall(function()
		scan_root(game:GetService(svc_name), svc_name, false)
	end)
end

if typeof(getnilinstances) == "function" then
	local ok_n, nils = pcall(getnilinstances)
	if ok_n and type(nils) == "table" then
		for _, inst in ipairs(nils) do
			pcall(function() scan_root(inst, "nil", true) end)
		end
	end
end

return hs:JSONEncode(out)

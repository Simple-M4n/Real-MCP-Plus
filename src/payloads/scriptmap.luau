local hs = game:GetService("HttpService")

local INCLUDE_NIL = __INCLUDE_NIL__
local TEST_DECOMPILE = __TEST_DECOMPILE__
local FILTER = string.lower(__FILTER__)
local MAX_ITEMS = __MAX_ITEMS__

local out = {
	scripts = {},
	counts = {
		total = 0,
		local_scripts = 0,
		module_scripts = 0,
		server_scripts = 0,
		nil_scripts = 0,
		in_actors = 0,
		decompile_failed = 0,
	},
	truncated = false,
}

local can_decompile = TEST_DECOMPILE and typeof(decompile) == "function"
local seen = {}

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function record(inst, origin, is_nil)
	if not inst:IsA("LuaSourceContainer") then return end
	local path = full_path(inst)
	local key = (is_nil and "nil:" or "g:") .. path
	if seen[key] then return end
	seen[key] = true

	if #FILTER > 0 then
		local hay = string.lower(inst.Name .. " " .. path)
		if not string.find(hay, FILTER, 1, true) then
			return
		end
	end

	out.counts.total += 1
	if inst:IsA("LocalScript") then
		out.counts.local_scripts += 1
	elseif inst:IsA("ModuleScript") then
		out.counts.module_scripts += 1
	elseif inst:IsA("Script") then
		out.counts.server_scripts += 1
	end
	if is_nil then
		out.counts.nil_scripts += 1
	end
	local in_actor = inst:FindFirstAncestorOfClass("Actor") ~= nil
	if in_actor then
		out.counts.in_actors += 1
	end

	if #out.scripts >= MAX_ITEMS then
		out.truncated = true
		return
	end

	local entry = {
		origin = origin,
		is_nil = is_nil,
		in_actor = in_actor,
		class = inst.ClassName,
		name = inst.Name,
		path = path,
	}

	if inst:IsA("LocalScript") or inst:IsA("Script") then
		local ok_en, en = pcall(function() return inst.Enabled end)
		if ok_en then entry.enabled = en end
	end

	if can_decompile then
		local ok_dec, src = pcall(decompile, inst)
		if ok_dec and type(src) == "string" and #src > 0 then
			entry.readable = true
			entry.source_len = #src
		else
			entry.readable = false
			out.counts.decompile_failed += 1
		end
	end

	table.insert(out.scripts, entry)
end

local function scan_root(root, origin, is_nil)
	if not root then return end
	record(root, origin, is_nil)
	local ok, list = pcall(function() return root:GetDescendants() end)
	if not ok or type(list) ~= "table" then return end
	for i, inst in ipairs(list) do
		if i % 5000 == 0 then task.wait() end
		record(inst, origin, is_nil)
	end
end

local ok_svcs, svcs = pcall(function() return game:GetChildren() end)
if ok_svcs and type(svcs) == "table" then
	for _, svc in ipairs(svcs) do
		pcall(function() scan_root(svc, svc.Name, false) end)
	end
end

if INCLUDE_NIL and typeof(getnilinstances) == "function" then
	local ok_n, nils = pcall(getnilinstances)
	if ok_n and type(nils) == "table" then
		for _, inst in ipairs(nils) do
			pcall(function() scan_root(inst, "nil", true) end)
		end
	end
end

return hs:JSONEncode(out)

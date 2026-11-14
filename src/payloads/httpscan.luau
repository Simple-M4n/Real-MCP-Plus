local hs = game:GetService("HttpService")

local SCAN_INSTANCES = __SCAN_INSTANCES__
local HOOK_REQUESTS = __HOOK_REQUESTS__

local out = {
	instance_strings = {},
	hook_status = nil,
	captured_requests = {},
}

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function looks_interesting(s)
	if type(s) ~= "string" or #s < 6 then return false end
	local low = string.lower(s)
	return string.find(low, "http://", 1, true) ~= nil
		or string.find(low, "https://", 1, true) ~= nil
		or string.find(low, "webhook", 1, true) ~= nil
		or string.find(low, "discord", 1, true) ~= nil
		or string.find(low, "telegram", 1, true) ~= nil
		or string.find(low, "firebase", 1, true) ~= nil
		or string.find(low, "pastebin", 1, true) ~= nil
		or string.find(low, "api.", 1, true) ~= nil
end

if SCAN_INSTANCES then
	local function scan_root(root, label)
		if not root then return end
		local ok, list = pcall(function() return root:GetDescendants() end)
		if not ok or type(list) ~= "table" then return end
		for _, inst in ipairs(list) do
			if #out.instance_strings >= 200 then break end
			if inst:IsA("StringValue") then
				local ok_v, val = pcall(function() return inst.Value end)
				if ok_v and looks_interesting(val) then
					table.insert(out.instance_strings, {
						source = label,
						kind = "StringValue",
						path = full_path(inst),
						value = string.sub(val, 1, 300),
					})
				end
			end
			local ok_a, attrs = pcall(function() return inst:GetAttributes() end)
			if ok_a and type(attrs) == "table" then
				for k, v in pairs(attrs) do
					if type(v) == "string" and looks_interesting(v) then
						table.insert(out.instance_strings, {
							source = label,
							kind = "Attribute:" .. tostring(k),
							path = full_path(inst),
							value = string.sub(v, 1, 300),
						})
					end
				end
			end
		end
	end

	local ok_svcs, svcs = pcall(function() return game:GetChildren() end)
	if ok_svcs and type(svcs) == "table" then
		for _, svc in ipairs(svcs) do
			pcall(function() scan_root(svc, svc.Name) end)
		end
	end

	if typeof(getnilinstances) == "function" then
		local ok_n, nils = pcall(getnilinstances)
		if ok_n and type(nils) == "table" then
			for _, inst in ipairs(nils) do
				pcall(function() scan_root(inst, "nil") end)
			end
		end
	end
end

if HOOK_REQUESTS then
	local env = getgenv and getgenv() or _G
	if type(env.__REALMCP_HTTP_LOG) == "table" then
		out.hook_status = "already_active"
		for _, entry in ipairs(env.__REALMCP_HTTP_LOG) do
			table.insert(out.captured_requests, entry)
		end
	elseif typeof(hookfunction) == "function" then
		env.__REALMCP_HTTP_LOG = {}
		local hooked = {}
		local function wrap_req(name, fn)
			if typeof(fn) ~= "function" then return end
			local ok, _ = pcall(function()
				local old
				old = hookfunction(fn, newcclosure(function(arg1, ...)
					pcall(function()
						if #env.__REALMCP_HTTP_LOG < 100 then
							local url = type(arg1) == "table" and (arg1.Url or arg1.url) or tostring(arg1)
							local method = type(arg1) == "table" and (arg1.Method or arg1.method) or "GET"
							table.insert(env.__REALMCP_HTTP_LOG, {
								fn = name,
								method = tostring(method),
								url = tostring(url),
							})
						end
					end)
					return old(arg1, ...)
				end))
			end)
			if ok then
				table.insert(hooked, name)
			end
		end

		wrap_req("request", request)
		wrap_req("http_request", http_request)
		if type(http) == "table" then
			wrap_req("http.request", http.request)
		end
		out.hook_status = (#hooked > 0) and ("installed:" .. table.concat(hooked, ",")) or "no_http_globals"
	else
		out.hook_status = "hookfunction_unavailable"
	end
end

return hs:JSONEncode(out)

local hs = game:GetService("HttpService")
local log_svc = game:GetService("LogService")
local lp = game:GetService("Players").LocalPlayer

local ACTION = __ACTION__
local env = getgenv and getgenv() or _G

local function full_path(inst)
	local ok, p = pcall(function() return inst:GetFullName() end)
	return ok and p or tostring(inst)
end

local function cleanup(state)
	if type(state) ~= "table" then return end
	if type(state.conns) == "table" then
		for _, c in ipairs(state.conns) do
			pcall(function() c:Disconnect() end)
		end
	end
end

if ACTION == "start" then
	cleanup(env.__REALMCP_WATCH)
	local state = {
		conns = {},
		console = {},
		ui_added = {},
		workspace_added = {},
	}

	table.insert(state.conns, log_svc.MessageOut:Connect(function(msg, mtype)
		if #state.console < 100 then
			table.insert(state.console, {
				type = tostring(mtype),
				message = string.sub(tostring(msg), 1, 240),
			})
		end
	end))

	if lp then
		local pgui = lp:FindFirstChild("PlayerGui")
		if pgui then
			table.insert(state.conns, pgui.DescendantAdded:Connect(function(inst)
				if #state.ui_added < 100 then
					table.insert(state.ui_added, {
						class = inst.ClassName,
						name = inst.Name,
						path = full_path(inst),
					})
				end
			end))
		end
	end

	table.insert(state.conns, workspace.DescendantAdded:Connect(function(inst)
		if #state.workspace_added < 100 then
			table.insert(state.workspace_added, {
				class = inst.ClassName,
				name = inst.Name,
				path = full_path(inst),
			})
		end
	end))

	env.__REALMCP_WATCH = state
	return hs:JSONEncode({ status = "started" })
elseif ACTION == "collect" then
	local state = env.__REALMCP_WATCH
	cleanup(state)
	env.__REALMCP_WATCH = nil
	if type(state) ~= "table" then
		return hs:JSONEncode({ error = "watch_not_started" })
	end
	return hs:JSONEncode({
		console = state.console,
		ui_added = state.ui_added,
		workspace_added = state.workspace_added,
	})
end

return hs:JSONEncode({ error = "unknown_action" })

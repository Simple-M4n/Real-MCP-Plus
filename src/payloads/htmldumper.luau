local hs = game:GetService("HttpService")
local lp = game:GetService("Players").LocalPlayer

local MODE = "__MODE__"
local TARGET = "__TARGET__"
local MAX_DEPTH = __MAX_DEPTH__
local INCLUDE_SOURCE = __INCLUDE_SOURCE__

local total_scanned = 0

local function safe_prop(inst, prop)
    local ok, val = pcall(function() return inst[prop] end)
    if ok and val ~= nil then
        if typeof(val) == "Vector3" then
            return { math.floor(val.X*100)/100, math.floor(val.Y*100)/100, math.floor(val.Z*100)/100 }
        elseif typeof(val) == "Color3" then
            return { math.floor(val.R*255), math.floor(val.G*255), math.floor(val.B*255) }
        elseif typeof(val) == "UDim2" then
            return tostring(val)
        elseif typeof(val) == "EnumItem" then
            return tostring(val.Name)
        else
            return val
        end
    end
    return nil
end

-- -------------------------------------------------------------
-- MODE: Structure (names, classNames, paths, hierarchy only)
-- -------------------------------------------------------------
local function scan_structure(inst, depth)
    if not inst or depth > MAX_DEPTH then return nil end
    total_scanned = total_scanned + 1

    local node = {
        name = inst.Name,
        className = inst.ClassName,
        path = inst:GetFullName()
    }

    local ch = {}
    for _, c in ipairs(inst:GetChildren()) do
        local child_node = scan_structure(c, depth + 1)
        if child_node then table.insert(ch, child_node) end
    end
    if #ch > 0 then node.children = ch end
    return node
end

-- -------------------------------------------------------------
-- MODE: All (full hierarchy, properties, attributes, scripts)
-- -------------------------------------------------------------
local function scan_all(inst, depth)
    if not inst or depth > MAX_DEPTH then return nil end
    total_scanned = total_scanned + 1

    local node = {
        name = inst.Name,
        className = inst.ClassName,
        path = inst:GetFullName()
    }

    local props = {}
    if inst:IsA("BasePart") then
        props.Anchored = safe_prop(inst, "Anchored")
        props.CanCollide = safe_prop(inst, "CanCollide")
        props.Transparency = safe_prop(inst, "Transparency")
        props.Size = safe_prop(inst, "Size")
        props.Position = safe_prop(inst, "Position")
    elseif inst:IsA("ValueBase") then
        props.Value = tostring(safe_prop(inst, "Value"))
    elseif inst:IsA("GuiObject") then
        props.Visible = safe_prop(inst, "Visible")
        props.Position = safe_prop(inst, "Position")
        props.Size = safe_prop(inst, "Size")
        props.ZIndex = safe_prop(inst, "ZIndex")
        if inst:IsA("TextLabel") or inst:IsA("TextButton") or inst:IsA("TextBox") then
            props.Text = safe_prop(inst, "Text")
        end
    elseif inst:IsA("RemoteEvent") or inst:IsA("UnreliableRemoteEvent") then
        props.isRemoteEvent = true
    elseif inst:IsA("RemoteFunction") then
        props.isRemoteFunction = true
    elseif inst:IsA("Sound") then
        props.SoundId = safe_prop(inst, "SoundId")
        props.Volume = safe_prop(inst, "Volume")
        props.IsPlaying = safe_prop(inst, "IsPlaying")
    end

    if inst:IsA("LuaSourceContainer") and INCLUDE_SOURCE then
        local src = nil
        pcall(function()
            if typeof(decompile) == "function" then
                src = decompile(inst)
            end
        end)
        if src and #src > 0 then
            if #src > 50000 then src = src:sub(1, 50000) .. "\n-- [Truncated...]" end
            props.source = src
            props.sourceBytes = #src
        end
    end

    local attr = inst:GetAttributes()
    if next(attr) then props.attributes = attr end
    if next(props) then node.properties = props end

    local ch = {}
    for _, c in ipairs(inst:GetChildren()) do
        local child_node = scan_all(c, depth + 1)
        if child_node then table.insert(ch, child_node) end
    end
    if #ch > 0 then node.children = ch end
    return node
end

-- -------------------------------------------------------------
-- MODE: Script (only LocalScript & ModuleScript hierarchy & source)
-- -------------------------------------------------------------
local function scan_scripts(inst, depth)
    if not inst or depth > MAX_DEPTH then return nil end

    local is_script = inst:IsA("LocalScript") or inst:IsA("ModuleScript")
    local ch = {}
    for _, c in ipairs(inst:GetChildren()) do
        local sub = scan_scripts(c, depth + 1)
        if sub then table.insert(ch, sub) end
    end

    if is_script or #ch > 0 then
        total_scanned = total_scanned + 1
        local node = {
            name = inst.Name,
            className = inst.ClassName,
            path = inst:GetFullName(),
            isScript = is_script
        }
        if is_script then
            local src = nil
            pcall(function()
                if typeof(decompile) == "function" then
                    src = decompile(inst)
                end
            end)
            node.hasSource = (src ~= nil and #src > 0)
            if src then
                if #src > 60000 then src = src:sub(1, 60000) .. "\n-- [Truncated...]" end
                node.source = src
                local _, lines = src:gsub("\n", "\n")
                node.lineCount = lines + 1
            else
                node.source = "-- [Decompile unavailable or empty]"
                node.lineCount = 0
            end
        end
        if #ch > 0 then node.children = ch end
        return node
    end
    return nil
end

-- -------------------------------------------------------------
-- MODE: Humanoid (living entities, NPCs, players, HP, stats)
-- -------------------------------------------------------------
local function scan_humanoids()
    local entities = {}
    local ws = workspace

    for _, desc in ipairs(ws:GetDescendants()) do
        if desc:IsA("Humanoid") and desc.Parent and desc.Parent:IsA("Model") then
            total_scanned = total_scanned + 1
            local model = desc.Parent
            local root = model:FindFirstChild("HumanoidRootPart") or model:FindFirstChild("Torso") or model.PrimaryPart
            local pos = nil
            if root and root:IsA("BasePart") then
                pos = { math.floor(root.Position.X*10)/10, math.floor(root.Position.Y*10)/10, math.floor(root.Position.Z*10)/10 }
            end

            local is_player = false
            local is_lp = false
            local plrs = game:GetService("Players")
            local plr = plrs:GetPlayerFromCharacter(model)
            if plr then
                is_player = true
                if plr == lp then is_lp = true end
            end

            local tools = {}
            for _, c in ipairs(model:GetChildren()) do
                if c:IsA("Tool") then table.insert(tools, c.Name) end
            end

            table.insert(entities, {
                name = model.Name,
                path = model:GetFullName(),
                className = model.ClassName,
                health = math.floor(desc.Health * 10) / 10,
                maxHealth = math.floor(desc.MaxHealth * 10) / 10,
                walkSpeed = desc.WalkSpeed,
                jumpPower = desc.JumpPower,
                position = pos,
                isPlayer = is_player,
                isLocalPlayer = is_lp,
                tools = tools,
                hasPrompt = model:FindFirstChildWhichIsA("ProximityPrompt", true) ~= nil
            })
        end
    end
    return entities
end

-- -------------------------------------------------------------
-- MODE: UI (PlayerGui, StarterGui, CoreGui structure & widgets)
-- -------------------------------------------------------------
local function scan_ui_node(inst, depth)
    if not inst or depth > MAX_DEPTH then return nil end
    total_scanned = total_scanned + 1

    local node = {
        name = inst.Name,
        className = inst.ClassName,
        path = inst:GetFullName()
    }

    if inst:IsA("GuiObject") then
        node.visible = safe_prop(inst, "Visible")
        node.position = safe_prop(inst, "Position")
        node.size = safe_prop(inst, "Size")
        node.zIndex = safe_prop(inst, "ZIndex")
        if inst:IsA("TextLabel") or inst:IsA("TextButton") or inst:IsA("TextBox") then
            node.text = safe_prop(inst, "Text")
            node.textColor = safe_prop(inst, "TextColor3")
        end
        if inst:IsA("ImageLabel") or inst:IsA("ImageButton") then
            node.image = safe_prop(inst, "Image")
        end
    elseif inst:IsA("ScreenGui") then
        node.enabled = safe_prop(inst, "Enabled")
    end

    local ch = {}
    for _, c in ipairs(inst:GetChildren()) do
        if c:IsA("GuiBase2d") or c:IsA("ScreenGui") or c:IsA("UIComponent") then
            local sub = scan_ui_node(c, depth + 1)
            if sub then table.insert(ch, sub) end
        end
    end
    if #ch > 0 then node.children = ch end
    return node
end

local function scan_all_ui()
    local res = {}
    local pgui = lp and lp:FindFirstChild("PlayerGui")
    if pgui then
        res.PlayerGui = scan_ui_node(pgui, 1)
    end
    local sg = game:GetService("StarterGui")
    res.StarterGui = scan_ui_node(sg, 1)
    return res
end

-- -------------------------------------------------------------
-- Execution & Packaging
-- -------------------------------------------------------------
local result_data = {}

if MODE == "Structure" then
    if TARGET == "all" or TARGET == "ReplicatedStorage" then
        result_data.ReplicatedStorage = scan_structure(game:GetService("ReplicatedStorage"), 1)
    end
    if TARGET == "all" or TARGET == "Workspace" then
        result_data.Workspace = scan_structure(workspace, 1)
    end
    if TARGET == "all" or TARGET == "StarterGui" then
        result_data.StarterGui = scan_structure(game:GetService("StarterGui"), 1)
    end
    if TARGET == "all" or TARGET == "StarterPlayer" then
        result_data.StarterPlayer = scan_structure(game:GetService("StarterPlayer"), 1)
    end
    if (TARGET == "all" or TARGET == "PlayerGui") and lp and lp:FindFirstChild("PlayerGui") then
        result_data.PlayerGui = scan_structure(lp.PlayerGui, 1)
    end
elseif MODE == "All" then
    if TARGET == "all" or TARGET == "ReplicatedStorage" then
        result_data.ReplicatedStorage = scan_all(game:GetService("ReplicatedStorage"), 1)
    end
    if TARGET == "all" or TARGET == "Workspace" then
        result_data.Workspace = scan_all(workspace, 1)
    end
    if TARGET == "all" or TARGET == "StarterGui" then
        result_data.StarterGui = scan_all(game:GetService("StarterGui"), 1)
    end
    if TARGET == "all" or TARGET == "StarterPlayer" then
        result_data.StarterPlayer = scan_all(game:GetService("StarterPlayer"), 1)
    end
    if (TARGET == "all" or TARGET == "PlayerGui") and lp and lp:FindFirstChild("PlayerGui") then
        result_data.PlayerGui = scan_all(lp.PlayerGui, 1)
    end
elseif MODE == "Script" then
    if TARGET == "all" or TARGET == "ReplicatedStorage" then
        result_data.ReplicatedStorage = scan_scripts(game:GetService("ReplicatedStorage"), 1)
    end
    if TARGET == "all" or TARGET == "Workspace" then
        result_data.Workspace = scan_scripts(workspace, 1)
    end
    if TARGET == "all" or TARGET == "StarterPlayer" then
        result_data.StarterPlayer = scan_scripts(game:GetService("StarterPlayer"), 1)
    end
    if (TARGET == "all" or TARGET == "PlayerGui") and lp and lp:FindFirstChild("PlayerGui") then
        result_data.PlayerGui = scan_scripts(lp.PlayerGui, 1)
    end
elseif MODE == "Humanoid" then
    result_data.Entities = scan_humanoids()
elseif MODE == "UI" then
    result_data.UI = scan_all_ui()
else
    result_data.Workspace = scan_structure(workspace, 1)
end

return hs:JSONEncode({
    timestamp = os.time(),
    mode = MODE,
    target = TARGET,
    maxDepth = MAX_DEPTH,
    totalScanned = total_scanned,
    data = result_data
})

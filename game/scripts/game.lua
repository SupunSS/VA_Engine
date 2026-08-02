-- ============================================================================
-- Game Logic in Lua
-- ============================================================================
-- This script shows how to write game mechanics in Lua using the Engine API
-- Benefits: Hot-reload, no recompilation, faster iteration
-- 
-- This script will be called from GameLogic.cpp
-- ============================================================================

-- Global game state
local Game = {
    playerVehicle = 0,
    playerHealth = 100,
    maxHealth = 100,
    isPaused = false,
    elapsedTime = 0
}

-- ============================================================================
-- INITIALIZATION
-- ============================================================================

function Game:Initialize()
    Engine:Log("Game Lua initialization")
    
    -- Subscribe to events
    Engine:Subscribe("OnGameStarted", function()
        Engine:Log("Game has started!")
    end)
    
    Engine:Subscribe("OnGamePaused", function()
        Engine:Log("Game paused")
        self.isPaused = true
    end)
    
    Engine:Subscribe("OnGameResumed", function()
        Engine:Log("Game resumed")
        self.isPaused = false
    end)
end

-- ============================================================================
-- UPDATE (called every frame)
-- ============================================================================

function Game:Update(deltaTime)
    self.elapsedTime = self.elapsedTime + deltaTime
    
    if self.isPaused then
        return
    end
    
    -- Handle input
    self:HandleInput()
    
    -- Update player
    self:UpdatePlayer(deltaTime)
    
    -- Update NPCs
    self:UpdateNPCs(deltaTime)
    
    -- Update UI
    self:UpdateUI()
end

-- ============================================================================
-- INPUT HANDLING
-- ============================================================================

function Game:HandleInput()
    -- Vehicle spawn on F
    if Engine:IsKeyHeld(KeyCode.F) and self.playerVehicle == 0 then
        self:SpawnPlayerVehicle()
    end
    
    -- Pause on Escape
    if Engine:IsKeyHeld(KeyCode.Escape) then
        self.isPaused = not self.isPaused
    end
end

-- ============================================================================
-- PLAYER VEHICLE
-- ============================================================================

function Game:SpawnPlayerVehicle()
    local cameraPos = Engine:GetCameraPosition()
    cameraPos.y = cameraPos.y + 2
    
    self.playerVehicle = Engine:SpawnEntity("car_sport", cameraPos)
    
    if self.playerVehicle ~= 0 then
        Engine:Log("Player vehicle spawned: " .. self.playerVehicle)
    else
        Engine:Log("Failed to spawn player vehicle - prefab not found")
    end
end

function Game:UpdatePlayer(deltaTime)
    if self.playerVehicle == 0 then
        return
    end
    
    -- Get input for vehicle control
    local throttle = 0
    local brake = 0
    local steer = 0
    
    if Engine:IsKeyHeld(KeyCode.W) then throttle = 1 end
    if Engine:IsKeyHeld(KeyCode.S) then brake = 1 end
    if Engine:IsKeyHeld(KeyCode.A) then steer = -1 end
    if Engine:IsKeyHeld(KeyCode.D) then steer = 1 end
    
    -- TODO: Apply to vehicle controller
    -- Once exposed via API:
    -- Engine:GetVehicleController(self.playerVehicle):SetInput(throttle, brake, steer)
end

-- ============================================================================
-- NPCS
-- ============================================================================

function Game:UpdateNPCs(deltaTime)
    -- TODO: Update all NPC entities
    -- - Run pathfinding
    -- - Update animations
    -- - Handle AI behavior
end

-- ============================================================================
-- UI / DEBUG RENDERING
-- ============================================================================

function Game:UpdateUI()
    if not Engine:IsDeveloperMode() then
        return
    end
    
    -- Debug info every 1 second
    if math.floor(self.elapsedTime) % 1 == 0 then
        local cameraPos = Engine:GetCameraPosition()
        local msg = string.format(
            "Camera: (%.1f, %.1f, %.1f) | Player Vehicle: %d | Health: %d/%d",
            cameraPos.x, cameraPos.y, cameraPos.z,
            self.playerVehicle,
            self.playerHealth,
            self.maxHealth
        )
        -- TODO: Display via ImGui or HUD system
    end
end

-- ============================================================================
-- EVENTS
-- ============================================================================

function Game:OnEntityDestroyed(entityId)
    if entityId == self.playerVehicle then
        Engine:Log("Player vehicle destroyed!")
        self.playerVehicle = 0
    end
end

function Game:OnCollisionEnter(entityA, entityB)
    -- Handle collision between entities
    Engine:Log("Collision: " .. entityA .. " <-> " .. entityB)
end

-- ============================================================================
-- EXPORTED FUNCTIONS (called from C++)
-- ============================================================================

-- Called by GameLogic::Initialize()
function InitializeGame()
    Game:Initialize()
end

-- Called by GameLogic::Update(deltaTime, input)
function UpdateGame(deltaTime)
    Game:Update(deltaTime)
end

-- Called by GameLogic::Render()
function RenderGame()
    -- Game rendering logic
    -- Usually done via ImGui or custom UI system
end

Engine:Log("Game Lua script loaded successfully")
                       

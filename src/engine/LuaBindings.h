/**
 * Lua Bindings for Engine API
 * 
 * This file registers all engine functions and types to be callable from Lua scripts.
 * Game developers can write gameplay logic in Lua and have hot-reload support.
 * 
 * Example Lua usage:
 *   local car = Engine:SpawnEntity("car_sport", {x=0, y=1, z=0})
 *   Engine:SetPosition(car, {x=10, y=1, z=10})
 *   Engine:PlaySound("engine_start.wav")
 */

#pragma once

#include <engine/public/EngineAPI.h>
#include <engine/public/Types.h>
#include <sol/sol.hpp>
#include <memory>

/**
 * Register all engine functions with Lua
 * Call this during engine initialization
 */
void RegisterLuaBindings(sol::state& lua, VAPublic::IEngine* engine);

/**
 * Example of how to use bindings:
 * 
 * In C++:
 *   sol::state lua;
 *   RegisterLuaBindings(lua, engine);
 *   lua.script_file("scripts/game.lua");
 * 
 * In Lua (scripts/game.lua):
 *   -- Spawn entities
 *   local player = Engine:SpawnEntity("player", {x=0, y=1, z=0})
 *   
 *   -- Handle input
 *   function OnUpdate(deltaTime)
 *       if Engine:IsKeyHeld(KeyCode.W) then
 *           local pos = Engine:GetPosition(player)
 *           pos.y = pos.y + 1
 *           Engine:SetPosition(player, pos)
 *       end
 *   end
 *   
 *   -- Subscribe to events
 *   Engine:Subscribe("OnEntityDestroyed", function(entityId)
 *       print("Entity destroyed: " .. entityId)
 *   end)
 *   
 *   -- Play sound
 *   Engine:PlaySound("audio/effects/explosion.wav", {x=0, y=0, z=0})
 */

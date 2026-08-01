/**
 * Lua Bindings Implementation
 * 
 * Registers all engine functions, types, and constants with the Lua state
 */

#include "LuaBindings.h"
#include <iostream>

void RegisterLuaBindings(sol::state& lua, IEngine* engine) {
    // ========================================================================
    // REGISTER TYPES
    // ========================================================================

    // Vec3 type
    lua.new_usertype<Vec3>("Vec3",
        sol::constructors<Vec3(), Vec3(float, float, float)>(),
        "x", &Vec3::x,
        "y", &Vec3::y,
        "z", &Vec3::z
    );

    // Quaternion type
    lua.new_usertype<Quat>("Quat",
        sol::constructors<Quat(), Quat(float, float, float, float)>(),
        "x", &Quat::x,
        "y", &Quat::y,
        "z", &Quat::z,
        "w", &Quat::w
    );

    // Transform type
    lua.new_usertype<Transform>("Transform",
        sol::constructors<Transform()>(),
        "position", &Transform::position,
        "rotation", &Transform::rotation,
        "scale", &Transform::scale
    );

    // InputState type
    lua.new_usertype<InputState>("InputState",
        "mouseX", &InputState::mouseX,
        "mouseY", &InputState::mouseY,
        "mousePressed", &InputState::mousePressed
    );

    // RaycastHit type
    lua.new_usertype<RaycastHit>("RaycastHit",
        "entityId", &RaycastHit::entityId,
        "distance", &RaycastHit::distance,
        "hitPoint", &RaycastHit::hitPoint,
        "normal", &RaycastHit::normal,
        "hit", &RaycastHit::hit
    );

    // ========================================================================
    // REGISTER ENUMS
    // ========================================================================

    // KeyCode enum
    lua.create_named_table("KeyCode");
    lua["KeyCode"]["W"] = KeyCode::W;
    lua["KeyCode"]["A"] = KeyCode::A;
    lua["KeyCode"]["S"] = KeyCode::S;
    lua["KeyCode"]["D"] = KeyCode::D;
    lua["KeyCode"]["Space"] = KeyCode::Space;
    lua["KeyCode"]["LeftShift"] = KeyCode::LeftShift;
    lua["KeyCode"]["E"] = KeyCode::E;
    lua["KeyCode"]["F"] = KeyCode::F;
    lua["KeyCode"]["Escape"] = KeyCode::Escape;
    lua["KeyCode"]["Mouse0"] = KeyCode::Mouse0;
    lua["KeyCode"]["Mouse1"] = KeyCode::Mouse1;
    lua["KeyCode"]["Up"] = KeyCode::Up;
    lua["KeyCode"]["Down"] = KeyCode::Down;
    lua["KeyCode"]["Left"] = KeyCode::Left;
    lua["KeyCode"]["Right"] = KeyCode::Right;

    // ========================================================================
    // REGISTER ENGINE FUNCTIONS
    // ========================================================================

    lua.create_named_table("Engine");

    // Scene Management
    lua["Engine"]["LoadScene"] = [engine](const std::string& path) {
        engine->LoadScene(path);
    };

    lua["Engine"]["SaveScene"] = [engine](const std::string& path) {
        engine->SaveScene(path);
    };

    // Entity Management
    lua["Engine"]["SpawnEntity"] = [engine](const std::string& prefabName, const sol::table& posTable) {
        Vec3 pos(0.0f);
        if (posTable["x"] != sol::nil) pos.x = posTable["x"];
        if (posTable["y"] != sol::nil) pos.y = posTable["y"];
        if (posTable["z"] != sol::nil) pos.z = posTable["z"];
        return engine->SpawnEntity(prefabName, pos);
    };

    lua["Engine"]["DestroyEntity"] = [engine](EntityId entityId) {
        engine->DestroyEntity(entityId);
    };

    lua["Engine"]["IsEntityValid"] = [engine](EntityId entityId) {
        return engine->IsEntityValid(entityId);
    };

    // Transform Operations
    lua["Engine"]["SetPosition"] = [engine](EntityId entityId, const sol::table& posTable) {
        Vec3 pos(0.0f);
        if (posTable["x"] != sol::nil) pos.x = posTable["x"];
        if (posTable["y"] != sol::nil) pos.y = posTable["y"];
        if (posTable["z"] != sol::nil) pos.z = posTable["z"];
        engine->SetPosition(entityId, pos);
    };

    lua["Engine"]["GetPosition"] = [engine](EntityId entityId) -> sol::table {
        sol::state_view lua_state(engine->GetScripting().GetLuaState());
        Vec3 pos = engine->GetPosition(entityId);
        sol::table result = lua_state.create_table();
        result["x"] = pos.x;
        result["y"] = pos.y;
        result["z"] = pos.z;
        return result;
    };

    lua["Engine"]["SetRotation"] = [engine](EntityId entityId, const sol::table& rotTable) {
        Quat rot(1.0f, 0.0f, 0.0f, 0.0f);
        if (rotTable["x"] != sol::nil) rot.x = rotTable["x"];
        if (rotTable["y"] != sol::nil) rot.y = rotTable["y"];
        if (rotTable["z"] != sol::nil) rot.z = rotTable["z"];
        if (rotTable["w"] != sol::nil) rot.w = rotTable["w"];
        engine->SetRotation(entityId, rot);
    };

    lua["Engine"]["GetRotation"] = [engine](EntityId entityId) -> sol::table {
        sol::state_view lua_state(engine->GetScripting().GetLuaState());
        Quat rot = engine->GetRotation(entityId);
        sol::table result = lua_state.create_table();
        result["x"] = rot.x;
        result["y"] = rot.y;
        result["z"] = rot.z;
        result["w"] = rot.w;
        return result;
    };

    // Physics
    lua["Engine"]["ApplyImpulse"] = [engine](EntityId entityId, const sol::table& impulseTable) {
        Vec3 impulse(0.0f);
        if (impulseTable["x"] != sol::nil) impulse.x = impulseTable["x"];
        if (impulseTable["y"] != sol::nil) impulse.y = impulseTable["y"];
        if (impulseTable["z"] != sol::nil) impulse.z = impulseTable["z"];
        engine->ApplyImpulse(entityId, impulse);
    };

    lua["Engine"]["Raycast"] = [engine](const sol::table& originTable, const sol::table& dirTable, 
                                        float maxDist) -> sol::table {
        Vec3 origin(0.0f), direction(0.0f);
        if (originTable["x"] != sol::nil) origin.x = originTable["x"];
        if (originTable["y"] != sol::nil) origin.y = originTable["y"];
        if (originTable["z"] != sol::nil) origin.z = originTable["z"];
        if (dirTable["x"] != sol::nil) direction.x = dirTable["x"];
        if (dirTable["y"] != sol::nil) direction.y = dirTable["y"];
        if (dirTable["z"] != sol::nil) direction.z = dirTable["z"];

        RaycastHit hit = engine->Raycast(origin, direction, maxDist);
        
        sol::state_view lua_state(engine->GetScripting().GetLuaState());
        sol::table result = lua_state.create_table();
        result["hit"] = hit.hit;
        result["entityId"] = hit.entityId;
        result["distance"] = hit.distance;
        return result;
    };

    // Audio
    lua["Engine"]["PlaySound"] = [engine](const std::string& path, 
                                         const sol::object& posObj, 
                                         float volume = 1.0f) {
        Vec3 pos(0.0f);
        if (posObj.is<sol::table>()) {
            sol::table posTable = posObj.as<sol::table>();
            if (posTable["x"] != sol::nil) pos.x = posTable["x"];
            if (posTable["y"] != sol::nil) pos.y = posTable["y"];
            if (posTable["z"] != sol::nil) pos.z = posTable["z"];
            engine->PlaySound(path, &pos, volume);
        } else {
            engine->PlaySound(path, nullptr, volume);
        }
    };

    // Input
    lua["Engine"]["GetInput"] = [engine]() -> sol::table {
        sol::state_view lua_state(engine->GetScripting().GetLuaState());
        InputState input = engine->GetInput();
        sol::table result = lua_state.create_table();
        result["mouseX"] = input.mouseX;
        result["mouseY"] = input.mouseY;
        result["mousePressed"] = input.mousePressed;
        return result;
    };

    lua["Engine"]["IsKeyHeld"] = [engine](KeyCode key) {
        return engine->IsKeyHeld(key);
    };

    // Camera
    lua["Engine"]["GetCameraPosition"] = [engine]() -> sol::table {
        sol::state_view lua_state(engine->GetScripting().GetLuaState());
        Vec3 pos = engine->GetCameraPosition();
        sol::table result = lua_state.create_table();
        result["x"] = pos.x;
        result["y"] = pos.y;
        result["z"] = pos.z;
        return result;
    };

    lua["Engine"]["SetCameraPosition"] = [engine](const sol::table& posTable) {
        Vec3 pos(0.0f);
        if (posTable["x"] != sol::nil) pos.x = posTable["x"];
        if (posTable["y"] != sol::nil) pos.y = posTable["y"];
        if (posTable["z"] != sol::nil) pos.z = posTable["z"];
        engine->SetCameraPosition(pos);
    };

    // Scripting
    lua["Engine"]["ExecuteScript"] = [engine](const std::string& path) {
        engine->ExecuteScript(path);
    };

    lua["Engine"]["ReloadScript"] = [engine](const std::string& name) {
        engine->ReloadScript(name);
    };

    // Events
    lua["Engine"]["Subscribe"] = [engine](const std::string& eventName, const sol::function& callback) {
        engine->GetEvents().Subscribe(eventName, [callback](const void*) {
            callback();
        });
    };

    lua["Engine"]["Emit"] = [engine](const std::string& eventName) {
        engine->GetEvents().Emit(eventName);
    };

    // Lifecycle
    lua["Engine"]["GetDeltaTime"] = [engine]() {
        return engine->GetDeltaTime();
    };

    lua["Engine"]["GetElapsedTime"] = [engine]() {
        return engine->GetElapsedTime();
    };

    lua["Engine"]["IsDeveloperMode"] = [engine]() {
        return engine->IsDeveloperMode();
    };

    // Debug/Logging
    lua["Engine"]["Log"] = [engine](const std::string& message) {
        engine->Log(message);
    };

    lua["Engine"]["SetDebugRendering"] = [engine](bool enabled) {
        engine->SetDebugRendering(enabled);
    };

    lua["Engine"]["RequestExit"] = [engine]() {
        engine->RequestExit();
    };
}

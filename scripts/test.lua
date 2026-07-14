if myEntity == nil then
    myEntity = nil
    moveTimer = 0.0
end

function on_load()
    if myEntity == nil then
        log_info("Lua script starting...")
        myEntity = spawn_model("models/test.obj", 3.0, 3.0, 0.0)
        log_info("Spawned model entity with ID: " .. myEntity)
    end
end

function on_update(deltaTime)
    moveTimer = moveTimer + deltaTime
    local t = get_transform(myEntity)
    t.position = Vec3.new(math.sin(moveTimer) * 3.0, 3.0, 0.0)
end
myEntity = entity_id
     moveTimer = 0.0

     function on_load()
     end

     function on_update(deltaTime)
         moveTimer = moveTimer + deltaTime
         if moveTimer > 1.0 then
             moveTimer = 0.0
             print("entity " .. tostring(myEntity) .. " ticking")
         end
     end
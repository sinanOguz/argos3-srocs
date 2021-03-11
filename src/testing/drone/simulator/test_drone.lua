--[[ This function is executed every time you press the 'execute' button ]]

-- trajectory = { time: number -> { position: vector3, yaw: number }}
trajectory = {
   [1]  =  { vector3( 0,  0,   0),  0.0},
   [10] =  { vector3( 0,  0, 1.25),  0.0},
   [40] =  { vector3( 4,  4, 1.25),  0.0}
}

function init()
   time = 1
   robot.directional_leds.set_single_color(1, "red")
   robot.directional_leds.set_single_color(2, "red")
   robot.directional_leds.set_single_color(3, "red")
   robot.directional_leds.set_single_color(4, "red")
end

function step()
   for timestamp, path in pairs(trajectory) do
      if time == timestamp then
         robot.flight_system.set_target_pose(table.unpack(path))
         log(tostring(time) .. ": " .. tostring(robot.flight_system.position))
      end
   end
   time = time + 1
end

function reset()
   init()
end

function destroy()
end

---@see augustus_api

local monthly_tax_bonus = 75

function on_load()
    ui.log("Scenario script loaded!")
    ui.show_warning("Lua scenario script active!")
    -- ui.post_message("This is message!")
end

function on_tick()
    if game.day() == 0 and finance.treasury() < 2000 then
        ui.show_warning("Treasury running low!")
    end
end

function on_month()
    local months = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    }
    local name = months[game.month() + 1] or "?"

    ui.show_warning(name .. " " .. game.year() .. " - Pop: " .. city.population())

    finance.add_treasury(monthly_tax_bonus)

    if city.population() > 1500 then
        city.change_favor(1)
        ui.show_warning("Growing city pleased Caesar!")
    end
end

function on_year()
    ui.show_warning("Year " .. game.year() .. " begins!")
    city.change_health(5)
    finance.add_treasury(500)
end

function on_event(name)
    ui.show_warning("Event: " .. name)
    if name == "earthquake" then
        finance.add_treasury(3000)
        ui.show_warning("Disaster relief: +3000 Dn")
    elseif name == "fire" then
        finance.add_treasury(-200)
        ui.show_warning("Fire damage: -200 Dn")
    end
end

function on_victory()
    ui.show_warning("VICTORY! Glory to Rome!")
end

function on_defeat()
    ui.show_warning("DEFEAT... Rome is displeased.")
end

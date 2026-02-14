function on_load()
    ui.log("Valentia scenario loaded")
    ui.input_dialog({
        title = "The Senate's Offer",
        subtitle = "Choose wisely",
        text = "Accept a gift or gain favor with the gods?",
        buttons = {
            { label = "Accept Gold",  on_click = function() finance.add_treasury(1000) end },
            { label = "Pray to Gods", on_click = function() city.change_favor(10) end },
            { label = "Decline" }, -- just closes the dialog
        }
    })
end

function on_year()
    local pop = city.population()
    if pop >= 500 then
        ui.show_custom_message({
            title = "Growing City",
            text = "Your city has reached " .. pop .. " citizens. The Senate takes notice.",
        })
    end
end

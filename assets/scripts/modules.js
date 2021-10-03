log_info("Augustus: load modules started")

var modules = [
    "math",
    "main_menu"
]

for (var i in modules) {
    log_info("Loading module " + modules[i])
    load_js_module(":" + modules[i] + ".js")
}

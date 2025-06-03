// Module A - Tests initialization order

print("[Module A] Initializing...")

var moduleA_initialized = true
var moduleA_counter = 1

export func getModuleAStatus() {
    return "Module A initialized with counter: " + moduleA_counter
}

print("[Module A] Ready - counter =", moduleA_counter)
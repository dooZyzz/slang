// Module B - Depends on Module A

print("[Module B] Initializing...")

// Try to use Module A
let statusA = getModuleAStatus()
print("[Module B] Module A status:", statusA)

var moduleB_initialized = true
var moduleB_counter = 2

export func getModuleBStatus() {
    return "Module B initialized with counter: " + moduleB_counter
}

print("[Module B] Ready - counter =", moduleB_counter)
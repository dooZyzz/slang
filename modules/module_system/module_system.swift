// Module system introspection and utilities
// Provides runtime access to module information

// Get information about the current module
func current_module() {
    return __current_module__
}

// Get list of all loaded modules
func loaded_modules() {
    return __loaded_modules__()
}

// Get module by name
func get_module(name) {
    let modules = loaded_modules()
    for mod in modules {
        if mod.path == name {
            return mod
        }
    }
    return nil
}

// Get module exports
func get_exports(module) {
    if !module {
        return []
    }
    return __module_exports__(module)
}

// Check if module is loaded
func is_loaded(name) {
    return get_module(name) != nil
}

// Get module version
func get_version(module) {
    if !module || !module.version {
        return "unknown"
    }
    return module.version
}

// Get module state
func get_state(module) {
    if !module {
        return "unknown"
    }
    return __module_state__(module)
}

// Pretty print module info
func print_module_info(name) {
    let module = get_module(name)
    if !module {
        print("Module '${name}' not found")
        return
    }
    
    print("=== Module: ${name} ===")
    print("State: ${get_state(module)}")
    print("Version: ${get_version(module)}")
    print("Type: ${module.is_native ? 'native' : 'script'}")
    
    let exports = get_exports(module)
    print("Exports (${exports.length}):")
    for exp in exports {
        print("  - ${exp.name} (${exp.type})")
    }
}

// List all modules with a pattern
func find_modules(pattern) {
    let modules = loaded_modules()
    let matches = []
    
    for mod in modules {
        if mod.path.contains(pattern) {
            matches.push(mod)
        }
    }
    
    return matches
}

// Find modules that export a symbol
func find_by_export(symbol) {
    let modules = loaded_modules()
    let matches = []
    
    for mod in modules {
        let exports = get_exports(mod)
        for exp in exports {
            if exp.name == symbol {
                matches.push(mod)
                break
            }
        }
    }
    
    return matches
}

// Module dependency helpers
func depends_on(module, dependency) {
    // TODO: Implement when dependency tracking is available
    return false
}

// Check module capabilities
func has_capability(module, capability) {
    if !module {
        return false
    }
    
    switch capability {
        case "native":
            return module.is_native
        case "lazy":
            return module.is_lazy
        case "exports":
            return get_exports(module).length > 0
        default:
            return false
    }
}

// Module statistics
func get_stats(module) {
    if !module {
        return nil
    }
    return __module_stats__(module)
}

// Export all functions
export {
    current_module,
    loaded_modules,
    get_module,
    get_exports,
    is_loaded,
    get_version,
    get_state,
    print_module_info,
    find_modules,
    find_by_export,
    depends_on,
    has_capability,
    get_stats
}
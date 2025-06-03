// Module that exports functions creating closures
let moduleVar = "I'm from the module"

export func makeGreeter(greeting) {
    // This closure should capture both the parameter and module context
    return { name in
        print("${greeting}, ${name}! (${moduleVar})")
    }
}

export func makeCounter(start) {
    let count = start
    return {
        count = count + 1
        return count
    }
}

// Test that module-level functions work  
func moduleHelper() {
    print("Helper called")
}

export func testModuleContext() {
    moduleHelper()
    return { x in
        moduleHelper()
        return x * 2
    }
}
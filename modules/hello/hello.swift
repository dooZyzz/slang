// Simple hello module
print("Hello module loading...")

func greet(name) {
    print("greet called with:", name)
    print("Hello, ${name}!")
}

let MESSAGE = "Hello from module"

export {greet}
export {MESSAGE}
// Module that uses other modules
import hello
import {add, multiply} from test_imports

print("Dependency example module loading...")

func greetAndCalculate(name, a, b) {
    hello.greet(name)
    let sum = add(a, b)
    let product = multiply(a, b)
    print("Sum: ${sum}, Product: ${product}")
    return product
}

export {greetAndCalculate}
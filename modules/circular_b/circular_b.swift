// Module B - imports Module A (circular!)
print("Loading module B...")

import circular_a

func functionB() {
    print("Function B called")
    // Don't call functionA here to avoid infinite recursion
}

export {functionB}
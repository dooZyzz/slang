// Module A - imports Module B
print("Loading module A...")

import circular_b

func functionA() {
    print("Function A called")
    circular_b.functionB()
}

export {functionA}
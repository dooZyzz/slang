// Simple module test

func helper() {
    print("Helper function")
}

export func public_func() {
    print("Public function")
    helper()
}

print("Testing module functions:")
helper()
public_func()
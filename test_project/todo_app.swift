// A simple todo list application in SwiftLang

// Todo item factory
func createTodo(id, text, done) {
    return {
        id: id,
        text: text, 
        done: done
    }
}

// Todo list manager
func createTodoList() {
    let todos = []
    let nextId = 1
    
    // Add a new todo
    let add = { text in
        let todo = createTodo(nextId, text, false)
        todos.push(todo)
        nextId = nextId + 1
        return todo
    }
    
    // Mark todo as done
    let complete = { id in
        let found = todos.find({ todo in todo.id == id })
        if found != nil {
            found.done = true
            return true
        }
        return false
    }
    
    // Get all todos
    let getAll = {
        return todos
    }
    
    // Get active todos
    let getActive = {
        return todos.filter({ todo in !todo.done })
    }
    
    // Get completed todos
    let getCompleted = {
        return todos.filter({ todo in todo.done })
    }
    
    // Clear completed
    let clearCompleted = {
        todos = todos.filter({ todo in !todo.done })
    }
    
    // Return API
    return {
        add: add,
        complete: complete,
        getAll: getAll,
        getActive: getActive,
        getCompleted: getCompleted,
        clearCompleted: clearCompleted
    }
}

// UI Helper functions
func printTodo(todo) {
    let status = todo.done ? "[✓]" : "[ ]"
    print("${todo.id}. ${status} ${todo.text}")
}

func printTodos(title, todos) {
    print("\n${title}:")
    if todos.length == 0 {
        print("  (none)")
    } else {
        todos.forEach({ todo in printTodo(todo) })
    }
}

// Main application
print("=== SwiftLang Todo App ===")

let todoList = createTodoList()

// Add some todos
print("\nAdding todos...")
todoList.add("Learn SwiftLang")
todoList.add("Build a compiler")
todoList.add("Write documentation")
todoList.add("Create examples")

// Show all todos
printTodos("All Todos", todoList.getAll())

// Complete some todos
print("\nCompleting todos 1 and 3...")
todoList.complete(1)
todoList.complete(3)

// Show different views
printTodos("Active Todos", todoList.getActive())
printTodos("Completed Todos", todoList.getCompleted())

// Clear completed
print("\nClearing completed todos...")
todoList.clearCompleted()
printTodos("Todos After Clearing", todoList.getAll())

print("\n=== Todo App Demo Complete ===")
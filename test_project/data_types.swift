// Data types and structures module

// Person struct
export struct Person {
    name
    age
}

// Team struct
export struct Team {
    name
    members
}

// Extension for Person
extension Person {
    func introduce() {
        print("Hi, I'm ${self.name} and I'm ${self.age} years old.")
    }
    
    func isAdult() {
        return self.age >= 18
    }
}

// Factory functions
export func createTeam(name, members) {
    return Team(name, members)
}

export func createPerson(name, age) {
    return Person(name, age)
}

// Collection operations
export func findOldest(people) {
    if people.length == 0 {
        return nil
    }
    
    let oldest = people.reduce(people[0], { current, person in
        if person.age > current.age {
            person
        } else {
            current
        }
    })
    
    return oldest
}

export func filterAdults(people) {
    return people.filter({ p in p.age >= 18 })
}

// Example of closure-returning function
export func createAgeChecker(minAge) {
    return { person in person.age >= minAge }
}

// Nested data structure
export struct Company {
    name
    teams
}

export func createCompany(name, teams) {
    return Company(name, teams)
}
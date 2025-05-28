// Example module with initialization hooks
// This simulates a database module that needs setup/teardown

let connections = []
let max_connections = 10

// Initialize the database connection pool
func init_database() {
    print("[DB] Initializing database module...")
    
    // Simulate creating connection pool
    for i in 0..max_connections {
        connections.push({
            id: i,
            connected: false,
            query_count: 0
        })
    }
    
    print("[DB] Created ${max_connections} connections")
    return true  // Init succeeded
}

// Cleanup database resources
func cleanup_database() {
    print("[DB] Cleaning up database module...")
    
    // Close all connections
    for conn in connections {
        if conn.connected {
            print("[DB] Closing connection ${conn.id}")
            conn.connected = false
        }
    }
    
    connections.clear()
    print("[DB] Database cleanup complete")
}

// Get a database connection
func get_connection() {
    for conn in connections {
        if !conn.connected {
            conn.connected = true
            print("[DB] Allocated connection ${conn.id}")
            return conn
        }
    }
    
    print("[DB] ERROR: No available connections!")
    return nil
}

// Release a connection back to the pool
func release_connection(conn) {
    if conn && conn.connected {
        print("[DB] Released connection ${conn.id} (queries: ${conn.query_count})")
        conn.connected = false
        conn.query_count = 0
    }
}

// Execute a query
func query(conn, sql) {
    if !conn || !conn.connected {
        print("[DB] ERROR: Invalid connection")
        return nil
    }
    
    conn.query_count = conn.query_count + 1
    print("[DB] Executing query on connection ${conn.id}: ${sql}")
    
    // Simulate query result
    return {
        rows: [],
        affected: 0,
        success: true
    }
}

// Export public API
export {
    get_connection,
    release_connection,
    query
}
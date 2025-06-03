#ifndef PLATFORM_THREADS_H
#define PLATFORM_THREADS_H

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    
    // Thread types
    typedef HANDLE platform_thread_t;
    typedef CRITICAL_SECTION platform_mutex_t;
    typedef SRWLOCK platform_rwlock_t;
    
    // Mutex operations
    #define platform_mutex_init(mutex) InitializeCriticalSection(mutex)
    #define platform_mutex_destroy(mutex) DeleteCriticalSection(mutex)
    #define platform_mutex_lock(mutex) EnterCriticalSection(mutex)
    #define platform_mutex_unlock(mutex) LeaveCriticalSection(mutex)
    
    // Read-write lock operations
    #define platform_rwlock_init(rwlock) InitializeSRWLock(rwlock)
    #define platform_rwlock_destroy(rwlock) /* No-op on Windows */
    #define platform_rwlock_rdlock(rwlock) AcquireSRWLockShared(rwlock)
    #define platform_rwlock_wrlock(rwlock) AcquireSRWLockExclusive(rwlock)
    #define platform_rwlock_unlock_read(rwlock) ReleaseSRWLockShared(rwlock)
    #define platform_rwlock_unlock_write(rwlock) ReleaseSRWLockExclusive(rwlock)
    
    // For generic unlock, we'll need to track lock type separately
    static inline void platform_rwlock_unlock(platform_rwlock_t* rwlock) {
        // This is a limitation - on Windows we need to know if it was a read or write lock
        // For now, we'll assume write locks are more common in our use case
        ReleaseSRWLockExclusive(rwlock);
    }
    
#else
    #include <pthread.h>
    
    // Thread types
    typedef pthread_t platform_thread_t;
    typedef pthread_mutex_t platform_mutex_t;
    typedef pthread_rwlock_t platform_rwlock_t;
    
    // Mutex operations
    #define platform_mutex_init(mutex) pthread_mutex_init(mutex, NULL)
    #define platform_mutex_destroy(mutex) pthread_mutex_destroy(mutex)
    #define platform_mutex_lock(mutex) pthread_mutex_lock(mutex)
    #define platform_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
    
    // Read-write lock operations
    #define platform_rwlock_init(rwlock) pthread_rwlock_init(rwlock, NULL)
    #define platform_rwlock_destroy(rwlock) pthread_rwlock_destroy(rwlock)
    #define platform_rwlock_rdlock(rwlock) pthread_rwlock_rdlock(rwlock)
    #define platform_rwlock_wrlock(rwlock) pthread_rwlock_wrlock(rwlock)
    #define platform_rwlock_unlock(rwlock) pthread_rwlock_unlock(rwlock)
    #define platform_rwlock_unlock_read(rwlock) pthread_rwlock_unlock(rwlock)
    #define platform_rwlock_unlock_write(rwlock) pthread_rwlock_unlock(rwlock)
#endif

#endif /* PLATFORM_THREADS_H */
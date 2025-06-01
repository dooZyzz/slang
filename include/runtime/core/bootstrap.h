#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "runtime/modules/loader/module_loader.h"
#include "runtime/core/vm.h"

// Create the bootstrap loader with built-in functions
ModuleLoader* bootstrap_loader_create(VM* vm);

// Create the built-ins module
Module* bootstrap_create_builtins_module(VM* vm);

#endif // BOOTSTRAP_H
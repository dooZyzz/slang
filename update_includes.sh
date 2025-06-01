#!/bin/bash

# Update include paths for reorganized project structure

# VM includes -> runtime/core
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "vm/vm.h"|#include "runtime/core/vm.h"|g' \
    -e 's|#include "vm/object.h"|#include "runtime/core/object.h"|g' \
    -e 's|#include "vm/object_hash.h"|#include "runtime/core/object_hash.h"|g' \
    -e 's|#include "vm/string_pool.h"|#include "runtime/core/string_pool.h"|g' \
    -e 's|#include "vm/array.h"|#include "runtime/core/array.h"|g'

# Core runtime includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/bootstrap.h"|#include "runtime/core/bootstrap.h"|g' \
    -e 's|#include "runtime/coroutine.h"|#include "runtime/core/coroutine.h"|g'

# Module loader includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/module.h"|#include "runtime/modules/loader/module_loader.h"|g' \
    -e 's|#include "runtime/module_cache.h"|#include "runtime/modules/loader/module_cache.h"|g' \
    -e 's|#include "runtime/module_compiler.h"|#include "runtime/modules/loader/module_compiler.h"|g'

# Module format includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/module_format.h"|#include "runtime/modules/formats/module_format.h"|g' \
    -e 's|#include "runtime/module_archive.h"|#include "runtime/modules/formats/module_archive.h"|g' \
    -e 's|#include "runtime/module_bundle.h"|#include "runtime/modules/formats/module_bundle.h"|g'

# Module extension includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/module_hooks.h"|#include "runtime/modules/extensions/module_hooks.h"|g' \
    -e 's|#include "runtime/module_inspect.h"|#include "runtime/modules/extensions/module_inspect.h"|g'

# Module lifecycle includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/builtin_modules.h"|#include "runtime/modules/lifecycle/builtin_modules.h"|g'

# Package includes
find . -name "*.c" -o -name "*.h" | xargs sed -i '' \
    -e 's|#include "runtime/package.h"|#include "runtime/packages/package.h"|g' \
    -e 's|#include "runtime/package_manager.h"|#include "runtime/packages/package_manager.h"|g'

echo "Include paths updated!"
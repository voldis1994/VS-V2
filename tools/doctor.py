#!/usr/bin/env python3
import shutil, sys
ok = all(shutil.which(x) for x in ['cmake','node','python3'])
print('VS-V2 doctor:', 'OK' if ok else 'MISSING DEPS')
sys.exit(0 if ok else 1)

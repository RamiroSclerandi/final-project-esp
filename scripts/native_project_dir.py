# Exposes the project root to host tests as NATIVE_PROJECT_DIR, so fixture
# paths resolve no matter which working directory runs the test binary.
Import("env")

project_dir = env.subst("$PROJECT_DIR").replace("\\", "/")
env.Append(CPPDEFINES=[("NATIVE_PROJECT_DIR", env.StringifyMacro(project_dir))])

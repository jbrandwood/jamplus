setlocal
for /f "usebackq delims=" %%I in (`python -c "import sysconfig; print(sysconfig.get_path('include'))"`) do set PYTHON_INCLUDE_DIR=%%I
for /f "usebackq delims=" %%I in (`python -c "import sysconfig; print(sysconfig.get_config_var('LIBDIR'))"`) do set PYTHON_LIB_DIR=%%I
for /f "usebackq delims=" %%I in (`python -c "import sysconfig; print(sysconfig.get_config_var('LDLIBRARY'))"`) do set PYTHON_LIB_FILE=%%I
set PYTHON_LIB_FILE=%PYTHON_LIB_FILE:.dll=.lib%
if not exist %~dp0..\bin\win64\modules mkdir %~dp0..\bin\win64\modules
cl /nologo /O2 /Oi /Gy /GL /EHsc /Zi /D LUA_BUILD_AS_DLL /D LUA_LIB /I %~dp0luaplus/Src/LuaPlus/lua53-luaplus/src /I %~dp0luaplus/Src /I %PYTHON_INCLUDE_DIR% /Fe"%~dp0..\bin\win64\modules\python.dll" onejam-python.c /link /DLL /LIBPATH:%PYTHON_LIB_DIR% /LIBPATH:%~dp0..\bin\win64 %PYTHON_LIB_FILE% lua53.lib /EXPORT:luaopen_python
del /q onejam-python.obj

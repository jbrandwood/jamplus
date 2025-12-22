setlocal
mkdir %~dp0..\bin\win64
link -dll -def:lua53.def -nologo -noentry -machine:X64 -incremental:no -nodefaultlib -out:"%~dp0..\bin\win64\lua53.dll"

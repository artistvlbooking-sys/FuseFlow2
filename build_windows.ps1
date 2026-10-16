$ErrorActionPreference = "Stop"

$buildDir = Join-Path $PSScriptRoot "build"

cmake -S $PSScriptRoot -B $buildDir -G "Visual Studio 17 2022" -A x64
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

cmake --build $buildDir --config Release --target FuseFlow
if ($LASTEXITCODE -ne 0) { throw "FuseFlow build failed." }

Write-Host "Build complete. Look under: $buildDir\FuseFlow_artefacts\Release\"

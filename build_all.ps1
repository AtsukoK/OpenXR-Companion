# Builds the layer + editor and copies everything into .\dist
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
& "$root\layer\build.bat"
if ($LASTEXITCODE -ne 0) { throw "layer build failed" }
dotnet publish "$root\editor\OpenXRHandMenusEditor.csproj" -c Release -o "$root\editor\bin\publish" --nologo -v q
if ($LASTEXITCODE -ne 0) { throw "editor build failed" }
$dist = "$root\dist"
New-Item -ItemType Directory -Force $dist | Out-Null
Copy-Item "$root\layer\build\OpenXRHandMenus.dll" $dist -Force
Copy-Item "$root\package\*" $dist -Force -Recurse
& "$root\layer\build32.bat"
if ($LASTEXITCODE -ne 0) { throw "32-bit layer build failed" }
New-Item -ItemType Directory -Force "$dist\x86" | Out-Null
Copy-Item "$root\layer\build32\OpenXRHandMenus.dll" "$dist\x86" -Force
Copy-Item "$root\editor\bin\publish\*" $dist -Force -Recurse
Write-Host "dist ready: $dist"

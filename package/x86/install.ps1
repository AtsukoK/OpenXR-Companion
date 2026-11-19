# Registers the 32-bit OpenXR Companion layer, for 32-bit games such as Fallout: New Vegas / Fallout 3 (TTWVR) (needs admin).
$ErrorActionPreference = 'Stop'
$json = Join-Path $PSScriptRoot 'XR_APILAYER_NOVENDOR_openxr_companion.json'
$key = 'HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit'
if (-not (Test-Path $key)) { New-Item $key -Force | Out-Null }
(Get-Item $key).Property | Where-Object { ($_ -like '*XR_APILAYER_NOVENDOR_openxr_companion.json' -or $_ -like '*XR_APILAYER_NOVENDOR_openxr_hand_menus.json' -or $_ -like '*XR_APILAYER_NOVENDOR_vr_radial_overlay.json') } |
    ForEach-Object { Remove-ItemProperty $key -Name $_ }
New-ItemProperty $key -Name $json -PropertyType DWord -Value 0 -Force | Out-Null
Write-Host "Installed: $json"
Start-Sleep 1

# Removes the 32-bit OpenXR Companion layer registration (needs admin).
$key = 'HKLM:\SOFTWARE\WOW6432Node\Khronos\OpenXR\1\ApiLayers\Implicit'
if (Test-Path $key) {
    (Get-Item $key).Property | Where-Object { ($_ -like '*XR_APILAYER_NOVENDOR_openxr_companion.json' -or $_ -like '*XR_APILAYER_NOVENDOR_openxr_hand_menus.json' -or $_ -like '*XR_APILAYER_NOVENDOR_vr_radial_overlay.json') } |
        ForEach-Object { Remove-ItemProperty $key -Name $_; Write-Host "Removed: $_" }
}
Start-Sleep 1

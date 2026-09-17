# Build a WiX MSI installer for the Windows plugin DLL, matching the
# OpenRGB Effects Plugin installer layout (installs to Program Files\OpenRGB\plugins).
# Requires WiX 3.x (candle/light) — preinstalled on GitHub windows-2022 runners.
# Output: dist/OpenRGB_3D_Spatial_Plugin_Windows_64_msi.zip (contains the .msi)

$ErrorActionPreference = 'Stop'
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $Root

# WiX may not be on PATH on older runner images
if (-not (Get-Command candle.exe -ErrorAction SilentlyContinue)) {
    $env:Path += ';C:\Program Files (x86)\WiX Toolset v3.14\bin'
}

function Generate-NamespaceUUID {
    param(
        [guid]$Namespace,
        [string]$Name
    )
    $nameBytes = [System.Text.Encoding]::UTF8.GetBytes($Name)
    $combined  = $Namespace.ToByteArray() + $nameBytes
    $hash      = [System.Security.Cryptography.SHA1]::Create().ComputeHash($combined)
    $hash[6]   = ($hash[6] -band 0x0f) -bor 0x50
    $hash[8]   = ($hash[8] -band 0x3f) -bor 0x80
    return ([guid][byte[]]($hash[0..15])).ToString().ToUpper()
}

$ProductName = 'OpenRGB 3D Spatial Plugin'
$PnSansWs    = $ProductName -replace ' ', '_'
$Vendor      = 'OpenRGB'

# Stable upgrade code so newer MSIs upgrade older installs
$UrlNamespace = [guid]'6ba7b810-9dad-11d1-80b4-00c04fd430c8'
$Namespace    = Generate-NamespaceUUID $UrlNamespace 'https://openrgb.org'
$UpgradeCode  = Generate-NamespaceUUID ([guid]$Namespace) 'org.OpenRGB.openrgb_3d_spatial_plugin'

# Version from git tag (same rule as scripts/ci/plugin-version.sh)
$Version = '0.0.0'
$describe = (git describe --tags --always 2>$null)
if ($describe) {
    $candidate = ($describe -replace '^v', '') -split '-' | Select-Object -First 1
    if ($candidate -match '^\d+(\.\d+)+$') { $Version = $candidate }
}

$IconFile         = 'OpenRGB\qt\OpenRGB.ico'
$LicenseFile      = 'OpenRGB\scripts\License.rtf'
$BannerImage      = 'OpenRGB\scripts\banner.bmp'
$DialogBackground = 'OpenRGB\scripts\dialog_background.bmp'

$SrcDll = Join-Path $Root 'release\OpenRGB3DSpatialPlugin.dll'
if (-not (Test-Path $SrcDll)) {
    throw "Build output not found: $SrcDll"
}

Write-Host "Product:`t$ProductName"
Write-Host "Version:`t$Version"
Write-Host "UpgradeCode:`t$UpgradeCode"

$ComponentGuid = [guid]::NewGuid().ToString().ToUpper()

$Wxs = @"
<?xml version='1.0' encoding='windows-1252'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
    <Product Name='$ProductName' Manufacturer='$Vendor'
        Id='*'
        UpgradeCode='$UpgradeCode'
        Language='1033' Codepage='1252' Version='$Version'>
        <Package Keywords='Installer' Description='$ProductName Installer'
            Comments='Organize and control RGB devices in a 3D grid with spatial effects.' Manufacturer='$Vendor'
            InstallerVersion='200' Languages='1033' Compressed='yes' SummaryCodepage='1252' Platform='x64'/>
        <Media Id='1' Cabinet='plugin.cab' EmbedCab='yes'/>
        <Condition Message='This package supports Windows 64bit Only'>VersionNT64</Condition>
        <MajorUpgrade Schedule='afterInstallInitialize' AllowDowngrades='yes'/>
        <Icon Id='OpenRGBIcon' SourceFile='$IconFile'/>
        <Property Id='ARPPRODUCTICON' Value='OpenRGBIcon'/>
        <Property Id='ARPURLINFOABOUT' Value='https://gitlab.com/wolfieeewolf1/OpenRGB3DSpatialPlugin'/>
        <Property Id='WIXUI_INSTALLDIR' Value='INSTALLDIR'/>
        <UIRef Id='WixUI_InstallDir'/>
        <UIRef Id='WixUI_ErrorProgressText'/>
        <WixVariable Id='WixUILicenseRtf' Value='$LicenseFile'/>
        <WixVariable Id='WixUIBannerBmp' Value='$BannerImage'/>
        <WixVariable Id='WixUIDialogBmp' Value='$DialogBackground'/>

        <Directory Id='TARGETDIR' Name='SourceDir'>
            <Directory Id='ProgramFiles64Folder'>
                <Directory Id='$Vendor' Name='$Vendor'>
                    <Directory Id='INSTALLDIR' Name='plugins'>
                        <Component Id='${PnSansWs}Files' Guid='$ComponentGuid'>
                            <File Id='PluginDll' Source='release\OpenRGB3DSpatialPlugin.dll'/>
                        </Component>
                    </Directory>
                </Directory>
            </Directory>
        </Directory>
        <Feature Id='Complete' Title='$ProductName' Description='Install all $ProductName files.' Display='expand' Level='1' ConfigurableDirectory='INSTALLDIR'>
            <Feature Id='${PnSansWs}Complete' Title='$ProductName' Description='The complete package.' Level='1' AllowAdvertise='no' InstallDefault='local'>
                <ComponentRef Id='${PnSansWs}Files'/>
            </Feature>
        </Feature>
    </Product>
</Wix>
"@

$WxsFile = "$PnSansWs.wxs"
$Wxs | Out-File -FilePath $WxsFile -Encoding UTF8
Get-Content $WxsFile | Write-Host

& candle -arch x64 $WxsFile
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Path (Join-Path $Root 'dist') -Force | Out-Null
$MsiName = "${PnSansWs}_Windows_64.msi"
$MsiPath = Join-Path $Root "dist\$MsiName"
& light -sval -ext WixUIExtension "$PnSansWs.wixobj" -out $MsiPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# GitLab/OpenRGB plugin downloads ship the MSI inside a zip named *_msi_*.
# Mirror that on GitHub Releases so the filename shows it is the installer.
$MsiZipPath = Join-Path $Root "dist\${PnSansWs}_Windows_64_msi.zip"
if (Test-Path $MsiZipPath) { Remove-Item $MsiZipPath -Force }
Compress-Archive -Path $MsiPath -DestinationPath $MsiZipPath
Remove-Item $MsiPath -Force

Write-Host "Packaged: $MsiZipPath (contains $MsiName)"

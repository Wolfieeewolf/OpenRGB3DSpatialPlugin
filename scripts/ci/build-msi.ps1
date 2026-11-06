# Build a WiX MSI: plugin DLL + OpenRGB3DSpatialPresets content pack.
# Layout matches OpenRGB Effects Plugin for the DLL (Program Files\OpenRGB\plugins)
# and AppData for content (%AppData%\OpenRGB\plugins\settings\OpenRGB3DSpatialPlugin\).
#
# Setup types (WixUI_Mondo):
#   Typical  — DLL + effect looks + Action tools + Triggers + Movie Sync (no controllers)
#   Complete — Typical + every controller layout category
#   Custom   — Advanced feature tree (pick packs and controller brands)
#
# Requires WiX 3.x (candle/light/heat). Presets root: -PresetsRoot, $env:PRESETS_ROOT,
# or sibling ../OpenRGB3DSpatialPresets.
# Output: dist/OpenRGB_3D_Spatial_Plugin_Windows_64_msi.zip (contains the .msi)

[CmdletBinding()]
param(
    [string]$PresetsRoot = ''
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $Root

if (-not (Get-Command candle.exe -ErrorAction SilentlyContinue)) {
    $env:Path += ';C:\Program Files (x86)\WiX Toolset v3.14\bin'
}
foreach ($tool in @('candle.exe', 'light.exe', 'heat.exe')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "WiX tool not found on PATH: $tool"
    }
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

function Resolve-PresetsRoot {
    param([string]$Explicit)
    if ($Explicit -and (Test-Path $Explicit)) { return (Resolve-Path $Explicit).Path }
    if ($env:PRESETS_ROOT -and (Test-Path $env:PRESETS_ROOT)) {
        return (Resolve-Path $env:PRESETS_ROOT).Path
    }
    $sibling = Join-Path (Split-Path $Root -Parent) 'OpenRGB3DSpatialPresets'
    if (Test-Path $sibling) { return (Resolve-Path $sibling).Path }
    $nested = Join-Path $Root 'OpenRGB3DSpatialPresets'
    if (Test-Path $nested) { return (Resolve-Path $nested).Path }
    throw @"
Presets pack not found. Pass -PresetsRoot, set PRESETS_ROOT, or clone
https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets next to this repo
(or under $Root\OpenRGB3DSpatialPresets for CI).
"@
}

function Copy-ContentTree {
    param(
        [string]$SourceDir,
        [string]$DestDir
    )
    if (-not (Test-Path $SourceDir)) { return $false }
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
    $skipNames = [System.Collections.Generic.HashSet[string]]::new(
        [string[]]@('.git', '.github', '.wiki-seed', 'docs', 'scripts', '__pycache__'),
        [StringComparer]::OrdinalIgnoreCase)
    $skipExt = [System.Collections.Generic.HashSet[string]]::new(
        [string[]]@('.md', '.py', '.pyc', '.ps1', '.sh', '.bat', '.gitignore', '.gitattributes'),
        [StringComparer]::OrdinalIgnoreCase)

    Get-ChildItem -Path $SourceDir -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring($SourceDir.Length).TrimStart('\', '/')
        $parts = $rel -split '[\\/]'
        if ($parts | Where-Object { $skipNames.Contains($_) }) { return }
        if ($skipExt.Contains($_.Extension)) { return }
        if ($_.Name -in @('LICENSE', 'CONTRIBUTING.md', 'template.controller.json')) { return }
        $dest = Join-Path $DestDir $rel
        $destParent = Split-Path $dest -Parent
        if (-not (Test-Path $destParent)) {
            New-Item -ItemType Directory -Path $destParent -Force | Out-Null
        }
        Copy-Item -LiteralPath $_.FullName -Destination $dest -Force
    }
    return ((Get-ChildItem -Path $DestDir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count -gt 0)
}

function Invoke-HeatDir {
    param(
        [string]$SourceDir,
        [string]$ComponentGroup,
        [string]$DirectoryId,
        [string]$VarName,
        [string]$OutWxs
    )
    if (-not (Test-Path $SourceDir)) {
        throw "Heat source missing: $SourceDir"
    }
    $fileCount = (Get-ChildItem -Path $SourceDir -Recurse -File | Measure-Object).Count
    if ($fileCount -lt 1) {
        throw "Heat source empty: $SourceDir"
    }
    # -ag: path-stable component GUIDs across rebuilds; -srd: files live in DirectoryId.
    & heat.exe dir $SourceDir `
        -cg $ComponentGroup `
        -ag `
        -sfrag `
        -srd `
        -dr $DirectoryId `
        -var "var.$VarName" `
        -out $OutWxs
    if ($LASTEXITCODE -ne 0) { throw "heat failed for $ComponentGroup (exit $LASTEXITCODE)" }
}

$ProductName = 'OpenRGB 3D Spatial Plugin'
$PnSansWs    = $ProductName -replace ' ', '_'
$Vendor      = 'OpenRGB'

$UrlNamespace = [guid]'6ba7b810-9dad-11d1-80b4-00c04fd430c8'
$Namespace    = Generate-NamespaceUUID $UrlNamespace 'https://openrgb.org'
$UpgradeCode  = Generate-NamespaceUUID ([guid]$Namespace) 'org.OpenRGB.openrgb_3d_spatial_plugin'

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

$Presets = Resolve-PresetsRoot -Explicit $PresetsRoot
$StageRoot = Join-Path $Root '_msi_stage'
if (Test-Path $StageRoot) { Remove-Item -Recurse -Force $StageRoot }
New-Item -ItemType Directory -Path $StageRoot -Force | Out-Null

Write-Host "Product:`t$ProductName"
Write-Host "Version:`t$Version"
Write-Host "UpgradeCode:`t$UpgradeCode"
Write-Host "Presets:`t$Presets"

# --- Stage content (same folder names as AppData / presets repo) ---
$harvests = @()

function Add-Harvest {
    param(
        [string]$RelSource,
        [string]$StageRel,
        [string]$ComponentGroup,
        [string]$DirectoryId,
        [string]$VarName,
        [int]$FeatureLevel = 1,
        [string]$FeatureId,
        [string]$FeatureTitle,
        [string]$ParentFeature
    )
    $src = Join-Path $Presets $RelSource
    $dst = Join-Path $StageRoot $StageRel
    if (-not (Copy-ContentTree -SourceDir $src -DestDir $dst)) {
        Write-Warning "Skipping empty/missing content: $RelSource"
        return
    }
    $wxs = Join-Path $StageRoot "heat_$ComponentGroup.wxs"
    Invoke-HeatDir -SourceDir $dst -ComponentGroup $ComponentGroup `
        -DirectoryId $DirectoryId -VarName $VarName -OutWxs $wxs
    $script:harvests += [pscustomobject]@{
        ComponentGroup = $ComponentGroup
        DirectoryId    = $DirectoryId
        VarName        = $VarName
        StageDir       = $dst
        Wxs            = $wxs
        FeatureLevel   = $FeatureLevel
        FeatureId      = $FeatureId
        FeatureTitle   = $FeatureTitle
        ParentFeature  = $ParentFeature
    }
}

# Standard (Typical): Level 1
Add-Harvest 'effects\volume'       'effects\volume'       'EffectsVolume'      'DIR_EFFECTS_VOLUME'      'EffectsVolume'      1 'FeatEffectsVolume'      'Volume'       'FeatEffects'
Add-Harvest 'effects\audio'        'effects\audio'        'EffectsAudio'       'DIR_EFFECTS_AUDIO'       'EffectsAudio'       1 'FeatEffectsAudio'       'Audio'        'FeatEffects'
Add-Harvest 'effects\media'        'effects\media'        'EffectsMedia'       'DIR_EFFECTS_MEDIA'       'EffectsMedia'       1 'FeatEffectsMedia'       'Media'        'FeatEffects'
Add-Harvest 'effects\shader-field' 'effects\shader-field' 'EffectsShaderField' 'DIR_EFFECTS_SHADERFIELD' 'EffectsShaderField' 1 'FeatEffectsShaderField' 'Shader Field' 'FeatEffects'
Add-Harvest 'effects\patterns'     'effects\patterns'     'EffectsPatterns'    'DIR_EFFECTS_PATTERNS'    'EffectsPatterns'    1 'FeatEffectsPatterns'    'Patterns'     'FeatEffects'

Add-Harvest 'action-editor\toolbox' 'action-editor\toolbox' 'ActionToolbox' 'DIR_ACTION_TOOLBOX' 'ActionToolbox' 1 'FeatActionToolbox' 'Action Editor tools' 'FeatContent'
Add-Harvest 'triggers'              'triggers'              'TriggersPack'  'DIR_TRIGGERS'       'TriggersPack'  1 'FeatTriggers'      'Trigger actions'     'FeatContent'
Add-Harvest 'movie-sync'            'movie-sync'            'MovieSyncPack' 'DIR_MOVIESYNC'      'MovieSyncPack' 1 'FeatMovieSync'     'Movie Sync helpers'  'FeatContent'

# Controllers: Level 100 → Complete / Custom only (not Typical)
$controllerCats = @(
    @{ Rel = 'controllers\cpu-coolers';   Id = 'CpuCoolers';   Title = 'CPU coolers' },
    @{ Rel = 'controllers\fans';           Id = 'Fans';         Title = 'Fans' },
    @{ Rel = 'controllers\graphics-cards'; Id = 'GraphicsCards'; Title = 'Graphics cards' },
    @{ Rel = 'controllers\keyboards';     Id = 'Keyboards';    Title = 'Keyboards' },
    @{ Rel = 'controllers\mice';           Id = 'Mice';         Title = 'Mice' },
    @{ Rel = 'controllers\motherboards';  Id = 'Motherboards'; Title = 'Motherboards' },
    @{ Rel = 'controllers\speakers';       Id = 'Speakers';     Title = 'Speakers' }
)
foreach ($cat in $controllerCats) {
    $dirId = "DIR_CTRL_$($cat.Id)"
    Add-Harvest $cat.Rel $cat.Rel "Controllers$($cat.Id)" $dirId "Controllers$($cat.Id)" `
        100 "FeatCtrl$($cat.Id)" $cat.Title 'FeatControllers'
}

if ($harvests.Count -lt 1) {
    throw "No content harvested from presets at $Presets"
}

$DllComponentGuid = Generate-NamespaceUUID ([guid]$Namespace) 'org.OpenRGB.openrgb_3d_spatial_plugin.dll'

# Directory fragments for harvested roots (heat -srd places files in these Ids)
$dirXml = New-Object System.Text.StringBuilder
[void]$dirXml.AppendLine(@"
                        <Directory Id='DIR_EFFECTS' Name='effects'>
                            <Directory Id='DIR_EFFECTS_VOLUME' Name='volume'/>
                            <Directory Id='DIR_EFFECTS_AUDIO' Name='audio'/>
                            <Directory Id='DIR_EFFECTS_MEDIA' Name='media'/>
                            <Directory Id='DIR_EFFECTS_SHADERFIELD' Name='shader-field'/>
                            <Directory Id='DIR_EFFECTS_PATTERNS' Name='patterns'/>
                        </Directory>
                        <Directory Id='DIR_ACTION_EDITOR' Name='action-editor'>
                            <Directory Id='DIR_ACTION_TOOLBOX' Name='toolbox'/>
                        </Directory>
                        <Directory Id='DIR_TRIGGERS' Name='triggers'/>
                        <Directory Id='DIR_MOVIESYNC' Name='movie-sync'/>
                        <Directory Id='DIR_CONTROLLERS' Name='controllers'>
                            <Directory Id='DIR_CTRL_CpuCoolers' Name='cpu-coolers'/>
                            <Directory Id='DIR_CTRL_Fans' Name='fans'/>
                            <Directory Id='DIR_CTRL_GraphicsCards' Name='graphics-cards'/>
                            <Directory Id='DIR_CTRL_Keyboards' Name='keyboards'/>
                            <Directory Id='DIR_CTRL_Mice' Name='mice'/>
                            <Directory Id='DIR_CTRL_Motherboards' Name='motherboards'/>
                            <Directory Id='DIR_CTRL_Speakers' Name='speakers'/>
                        </Directory>
"@)

# Feature XML
function Feature-Refs([string]$ParentId) {
    $sb = New-Object System.Text.StringBuilder
    foreach ($h in ($harvests | Where-Object { $_.ParentFeature -eq $ParentId })) {
        [void]$sb.AppendLine("                <Feature Id='$($h.FeatureId)' Title='$($h.FeatureTitle)' Level='$($h.FeatureLevel)' AllowAdvertise='no' InstallDefault='local'>")
        [void]$sb.AppendLine("                    <ComponentGroupRef Id='$($h.ComponentGroup)'/>")
        [void]$sb.AppendLine('                </Feature>')
    }
    return $sb.ToString()
}

$effectsFeatures = Feature-Refs 'FeatEffects'
$contentLeaf = Feature-Refs 'FeatContent'
$controllerFeatures = Feature-Refs 'FeatControllers'

$Wxs = @"
<?xml version='1.0' encoding='windows-1252'?>
<Wix xmlns='http://schemas.microsoft.com/wix/2006/wi'>
    <Product Name='$ProductName' Manufacturer='$Vendor'
        Id='*'
        UpgradeCode='$UpgradeCode'
        Language='1033' Codepage='1252' Version='$Version'>
        <Package Keywords='Installer' Description='$ProductName Installer'
            Comments='Organize and control RGB devices in a 3D grid with spatial effects. Includes optional content pack.' Manufacturer='$Vendor'
            InstallerVersion='200' Languages='1033' Compressed='yes' SummaryCodepage='1252' Platform='x64'/>
        <Media Id='1' Cabinet='plugin.cab' EmbedCab='yes'/>
        <Condition Message='This package supports Windows 64bit Only'>VersionNT64</Condition>
        <MajorUpgrade Schedule='afterInstallInitialize' AllowDowngrades='yes'/>
        <Icon Id='OpenRGBIcon' SourceFile='$IconFile'/>
        <Property Id='ARPPRODUCTICON' Value='OpenRGBIcon'/>
        <Property Id='ARPURLINFOABOUT' Value='https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin'/>
        <Property Id='WIXUI_INSTALLDIR' Value='INSTALLDIR'/>
        <!-- Mondo: Typical = Standard content; Custom = Advanced pick-list; Complete = all controllers too -->
        <UIRef Id='WixUI_Mondo'/>
        <UIRef Id='WixUI_ErrorProgressText'/>
        <WixVariable Id='WixUILicenseRtf' Value='$LicenseFile'/>
        <WixVariable Id='WixUIBannerBmp' Value='$BannerImage'/>
        <WixVariable Id='WixUIDialogBmp' Value='$DialogBackground'/>

        <Directory Id='TARGETDIR' Name='SourceDir'>
            <Directory Id='ProgramFiles64Folder'>
                <Directory Id='$Vendor' Name='$Vendor'>
                    <Directory Id='INSTALLDIR' Name='plugins'>
                        <Component Id='${PnSansWs}Dll' Guid='$DllComponentGuid'>
                            <File Id='PluginDll' Source='release\OpenRGB3DSpatialPlugin.dll' KeyPath='yes'/>
                        </Component>
                    </Directory>
                </Directory>
            </Directory>
            <Directory Id='AppDataFolder'>
                <Directory Id='OpenRGBAppData' Name='OpenRGB'>
                    <Directory Id='OpenRGBPlugins' Name='plugins'>
                        <Directory Id='OpenRGBPluginSettings' Name='settings'>
                            <Directory Id='PLUGINDATA' Name='OpenRGB3DSpatialPlugin'>
$($dirXml.ToString())
                            </Directory>
                        </Directory>
                    </Directory>
                </Directory>
            </Directory>
        </Directory>

        <Feature Id='Complete' Title='$ProductName' Description='Plugin and content pack.' Display='expand' Level='1' ConfigurableDirectory='INSTALLDIR'>
            <Feature Id='FeatPlugin' Title='Plugin (required)' Description='OpenRGB3DSpatialPlugin.dll into OpenRGB\plugins.' Level='1' Absent='disallow' AllowAdvertise='no' InstallDefault='local'>
                <ComponentRef Id='${PnSansWs}Dll'/>
            </Feature>
            <Feature Id='FeatContent' Title='Content pack' Description='Looks from OpenRGB3DSpatialPresets (AppData). Typical skips controller layouts.' Display='expand' Level='1'>
                <Feature Id='FeatEffects' Title='Effect looks' Description='Volume, Audio, Media, Shader Field, Patterns.' Display='expand' Level='1'>
$effectsFeatures
                </Feature>
$contentLeaf
                <Feature Id='FeatControllers' Title='Controller layouts' Description='Install only devices you own. Off in Typical; on in Complete; pick in Custom.' Display='expand' Level='1'>
$controllerFeatures
                </Feature>
            </Feature>
        </Feature>
    </Product>
</Wix>
"@

$WxsFile = Join-Path $StageRoot "$PnSansWs.wxs"
$Wxs | Out-File -FilePath $WxsFile -Encoding UTF8

# candle with absolute .wxs paths writes .wixobj into the cwd — keep objs in the stage dir.
$candleArgs = @('-arch', 'x64', '-out', (Join-Path $StageRoot '\'))
foreach ($h in $harvests) {
    $candleArgs += "-d$($h.VarName)=$($h.StageDir)"
}
$candleArgs += $WxsFile
foreach ($h in $harvests) {
    $candleArgs += $h.Wxs
}

Write-Host "candle $($candleArgs -join ' ')"
& candle @candleArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Path (Join-Path $Root 'dist') -Force | Out-Null
$MsiName = "${PnSansWs}_Windows_64.msi"
$MsiPath = Join-Path $Root "dist\$MsiName"

$allObjs = @(Get-ChildItem -Path $StageRoot -Filter '*.wixobj' -File | ForEach-Object { $_.FullName })
if ($allObjs.Count -lt 1) {
    throw "candle produced no .wixobj under $StageRoot"
}

& light -sval -ext WixUIExtension -out $MsiPath @allObjs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$MsiZipPath = Join-Path $Root "dist\${PnSansWs}_Windows_64_msi.zip"
if (Test-Path $MsiZipPath) { Remove-Item $MsiZipPath -Force }
Compress-Archive -Path $MsiPath -DestinationPath $MsiZipPath
Remove-Item $MsiPath -Force

# Clean intermediate candle/heat outputs from repo root if any leaked
Get-ChildItem -Path $Root -Filter "$PnSansWs.*" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Extension -in '.wxs', '.wixobj', '.wixpdb' } |
    Remove-Item -Force -ErrorAction SilentlyContinue

Write-Host "Packaged: $MsiZipPath (contains $MsiName)"
Write-Host "Content features: $($harvests.Count) (controllers Level=100 = Complete/Custom)"

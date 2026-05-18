param(
    [int]$TimeoutSeconds = 20,
    [int]$HoldSeconds = 0
)

$ErrorActionPreference = "Stop"

if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) {
    $powershell = (Get-Command powershell.exe).Source
    & $powershell -STA -ExecutionPolicy Bypass -File $PSCommandPath -TimeoutSeconds $TimeoutSeconds -HoldSeconds $HoldSeconds
    exit $LASTEXITCODE
}

. "B:\usagi_dev\tools\usagi-dev-env.ps1"

$usagiRepoRoot = (Resolve-Path $PSScriptRoot).Path
while ($usagiRepoRoot -and !(Test-Path (Join-Path $usagiRepoRoot "Engine\CommonProps.props"))) {
    $usagiRepoRoot = Split-Path $usagiRepoRoot -Parent
}

if (!$usagiRepoRoot) {
    throw "Could not locate Usagi repository root from $PSScriptRoot"
}

$hostPath = Join-Path $usagiRepoRoot "Tools\bin\UsagiPreviewHost.exe"
if (!(Test-Path $hostPath)) {
    throw "Preview host not found: $hostPath"
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$form = New-Object System.Windows.Forms.Form
$form.Text = "Usagi Preview Host Smoke"
$form.Width = 640
$form.Height = 480
$form.Show()
[System.Windows.Forms.Application]::DoEvents()

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $hostPath
$romfilesRoot = Join-Path $usagiRepoRoot "_romfiles\win"
New-Item -ItemType Directory -Force -Path $romfilesRoot | Out-Null
$fallbackRomfilesRoot = Join-Path (Split-Path -Parent $usagiRepoRoot) "_romfiles\win"
$nameDataHash = Join-Path $romfilesRoot "nameDataHash.bin"
if (!(Test-Path $nameDataHash)) {
    $fallbackNameDataHash = Join-Path $fallbackRomfilesRoot "nameDataHash.bin"
    if (Test-Path $fallbackNameDataHash) {
        Copy-Item -LiteralPath $fallbackNameDataHash -Destination $nameDataHash -Force
    }
    else {
        throw "Preview smoke requires nameDataHash.bin. Run a data build first."
    }
}

function Copy-PreviewResource([string]$relativePath) {
    $source = Join-Path $fallbackRomfilesRoot $relativePath
    $destination = Join-Path $romfilesRoot $relativePath

    if (!(Test-Path $source)) {
        throw "Preview smoke requires compiled resource: $source"
    }

    if ((Get-Item -LiteralPath $source).PSIsContainer) {
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        Copy-Item -Path (Join-Path $source "*") -Destination $destination -Recurse -Force
    }
    else {
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination -Force
    }
}

function Ensure-PreviewModelResource {
    $modelPath = Join-Path $fallbackRomfilesRoot "Models\PBRSample\PBRSample.vmdf"
    if (Test-Path $modelPath) {
        return
    }

    $ayataka = Join-Path $usagiRepoRoot "Tools\bin\Ayataka.exe"
    if (!(Test-Path $ayataka)) {
        throw "Preview smoke requires Ayataka.exe to build the model fixture: $ayataka"
    }

    $modelSource = Join-Path $usagiRepoRoot "Data\Models\PBRSample\PBRSample.fbx"
    if (!(Test-Path $modelSource)) {
        throw "Preview smoke requires model source fixture: $modelSource"
    }

    $modelOutDir = Split-Path -Parent $modelPath
    $skeletonPath = Join-Path $usagiRepoRoot "_build\skel\PBRSample\PBRSample.vmdf.xml"
    New-Item -ItemType Directory -Force -Path $modelOutDir, (Split-Path -Parent $skeletonPath) | Out-Null

    & $ayataka `
        "-a16" `
        "-o$modelPath" `
        "-sk$modelOutDir\" `
        "-lh" `
        "-d$modelPath.d" `
        "-h$skeletonPath" `
        $modelSource
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    if (!(Test-Path $modelPath)) {
        throw "Ayataka did not produce the preview model fixture: $modelPath"
    }
}

Ensure-PreviewModelResource
Copy-PreviewResource "Particle"
Copy-PreviewResource "Effects"
Copy-PreviewResource "Textures"
Copy-PreviewResource "Models\PBRSample"

$startInfo.WorkingDirectory = $romfilesRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
$readTask = $null

function Send-Json($json) {
    $process.StandardInput.WriteLine($json)
    $process.StandardInput.Flush()
}

function Wait-ForLine([scriptblock]$predicate, [string]$description) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $seen = New-Object System.Collections.Generic.List[string]

    while ([DateTime]::UtcNow -lt $deadline) {
        [System.Windows.Forms.Application]::DoEvents()

        if ($script:readTask -eq $null) {
            $script:readTask = $process.StandardOutput.ReadLineAsync()
        }

        if ($script:readTask.Wait(50)) {
            $line = $script:readTask.Result
            $script:readTask = $null
            if ($line -eq $null) {
                break
            }

            $seen.Add($line)
            if (& $predicate $line) {
                return $line
            }
        }

        if ($process.HasExited) {
            break
        }

        Start-Sleep -Milliseconds 50
    }

    $exitDetails = ""
    if ($process.HasExited) {
        $exitDetails = "process exited with code $($process.ExitCode)."
    }

    $stderr = if ($process.HasExited) { $process.StandardError.ReadToEnd() } else { "" }

    throw "Timed out waiting for $description. $exitDetails`nstdout:`n$($seen -join "`n")`nstderr:`n$stderr"
}

function Measure-PreviewVariance {
    [System.Windows.Forms.Application]::DoEvents()
    Start-Sleep -Milliseconds 100

    $clientRect = $form.ClientRectangle
    $screenOrigin = $form.PointToScreen($clientRect.Location)
    $bitmap = [System.Drawing.Bitmap]::new($clientRect.Width, $clientRect.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($screenOrigin.X, $screenOrigin.Y, 0, 0, $clientRect.Size)

        $sampleStep = 4
        $first = $bitmap.GetPixel([Math]::Min(8, $clientRect.Width - 1), [Math]::Min(8, $clientRect.Height - 1)).ToArgb()
        $changed = 0
        for ($y = 0; $y -lt $clientRect.Height; $y += $sampleStep) {
            for ($x = 0; $x -lt $clientRect.Width; $x += $sampleStep) {
                if ($bitmap.GetPixel($x, $y).ToArgb() -ne $first) {
                    $changed++
                }
            }
        }

        return $changed
    }
    finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    [void]$process.Start()

    $dataPath = (Join-Path $usagiRepoRoot "Data").Replace("\", "\\")
    $romfilesPath = (Join-Path $usagiRepoRoot "_romfiles\win").Replace("\", "\\")
    Send-Json "{`"type`":`"init`",`"protocolVersion`":1,`"dataPath`":`"$dataPath`",`"romfilesPath`":`"$romfilesPath`"}"
    [void](Wait-ForLine { param($line) $line -match '"type":"ready"' } "ready response")

    $hwnd = $form.Handle.ToInt64()
    Send-Json "{`"type`":`"attachWindow`",`"hwnd`":$hwnd,`"width`":640,`"height`":480}"
    [void](Wait-ForLine { param($line) $line -match 'Usagi engine initialized for preview host' } "engine initialization diagnostic")

    $emitterPath = (Join-Path $usagiRepoRoot "Data\Particle\Emitters\multi_texture_slots.yml").Replace("\", "\\")
    $effectPath = (Join-Path $usagiRepoRoot "Data\Particle\Effects\multi_texture_slots.yml").Replace("\", "\\")
    Send-Json "{`"type`":`"loadParticle`",`"emitterPath`":`"$emitterPath`",`"effectPath`":`"$effectPath`"}"
    [void](Wait-ForLine { param($line) $line -match '"type":"loaded"' -and $line -match '"success":true' } "successful particle load response")

    for ($i = 0; $i -lt 60; $i++) {
        Send-Json "{`"type`":`"tick`",`"deltaTime`":0.0166667}"
        Start-Sleep -Milliseconds 16
        [System.Windows.Forms.Application]::DoEvents()
    }

    $changedPixels = Measure-PreviewVariance
    if ($changedPixels -lt 4) {
        throw "Preview window did not show enough pixel variance after particle load."
    }

    if ($HoldSeconds -gt 0) {
        $holdUntil = [DateTime]::UtcNow.AddSeconds($HoldSeconds)
        while ([DateTime]::UtcNow -lt $holdUntil) {
            Send-Json "{`"type`":`"tick`",`"deltaTime`":0.0001}"
            Start-Sleep -Milliseconds 50
            [System.Windows.Forms.Application]::DoEvents()
        }
    }

    $entityPath = (Join-Path $usagiRepoRoot "Data\Entities\PreviewPBRSample.yml")
    $tempEntityDir = Join-Path $usagiRepoRoot "_romfiles\preview"
    New-Item -ItemType Directory -Force -Path $tempEntityDir | Out-Null
    $entityPath = Join-Path $tempEntityDir "PreviewPBRSample.yml"
    @'
ModelComponent:
  name: PBRSample/PBRSample.vmdf
'@ | Set-Content -Encoding ASCII -Path $entityPath

    $escapedEntityPath = $entityPath.Replace("\", "\\")
    Send-Json "{`"type`":`"loadEntity`",`"path`":`"$escapedEntityPath`"}"
    [void](Wait-ForLine { param($line) $line -match '"type":"loaded"' -and $line -match '"resourceType":"entity"' -and $line -match '"success":true' } "successful entity load response")

    for ($i = 0; $i -lt 30; $i++) {
        Send-Json "{`"type`":`"tick`",`"deltaTime`":0.0166667}"
        Start-Sleep -Milliseconds 16
        [System.Windows.Forms.Application]::DoEvents()
    }

    $changedPixels = Measure-PreviewVariance
    if ($changedPixels -lt 4) {
        throw "Preview window did not show enough pixel variance after entity/model load."
    }

    Send-Json "{`"type`":`"shutdown`"}"
    if (!$process.WaitForExit(5000)) {
        $process.Kill()
        throw "Preview host did not exit after shutdown."
    }

    if ($process.ExitCode -ne 0) {
        $stderr = $process.StandardError.ReadToEnd()
        throw "Preview host exited with code $($process.ExitCode).`nstderr:`n$stderr"
    }

    Write-Host "Preview host smoke passed."
}
finally {
    if (!$process.HasExited) {
        try {
            Send-Json "{`"type`":`"shutdown`"}"
            if (!$process.WaitForExit(1000)) {
                $process.Kill()
            }
        }
        catch {
            try { $process.Kill() } catch { }
        }
    }

    $process.Dispose()
    $form.Close()
    $form.Dispose()
}

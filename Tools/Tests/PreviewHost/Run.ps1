param(
    [int]$TimeoutSeconds = 20
)

$ErrorActionPreference = "Stop"

if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne [Threading.ApartmentState]::STA) {
    $powershell = (Get-Command powershell.exe).Source
    & $powershell -STA -ExecutionPolicy Bypass -File $PSCommandPath -TimeoutSeconds $TimeoutSeconds
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

Copy-PreviewResource "Particle"
Copy-PreviewResource "Effects"
Copy-PreviewResource "Textures"

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

    $bounds = $form.Bounds
    $bitmap = [System.Drawing.Bitmap]::new($bounds.Width, $bounds.Height)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($bounds.Left, $bounds.Top, 0, 0, $bounds.Size)

        $sampleStep = 16
        $first = $bitmap.GetPixel([Math]::Min(8, $bounds.Width - 1), [Math]::Min(8, $bounds.Height - 1)).ToArgb()
        $changed = 0
        for ($y = 0; $y -lt $bounds.Height; $y += $sampleStep) {
            for ($x = 0; $x -lt $bounds.Width; $x += $sampleStep) {
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

    for ($i = 0; $i -lt 12; $i++) {
        Send-Json "{`"type`":`"tick`",`"deltaTime`":0.0166667}"
        Start-Sleep -Milliseconds 50
        [System.Windows.Forms.Application]::DoEvents()
    }

    $changedPixels = Measure-PreviewVariance
    if ($changedPixels -lt 4) {
        throw "Preview window did not show enough pixel variance after particle load."
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

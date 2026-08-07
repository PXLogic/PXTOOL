$ErrorActionPreference = 'Stop'

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$cleanupScript = Join-Path $PSScriptRoot 'cleanup_stale_install_checks.sh'
$fullBuildScript = Join-Path $PSScriptRoot 'FULL_BUILD.bat'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pxtool-stale-install-cleanup-" + [guid]::NewGuid())

try {
    New-Item -ItemType Directory -Path (Join-Path $testRoot 'install-webui-check-full') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $testRoot 'install-webui-check-minimal') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $testRoot 'keep-this-directory') -Force | Out-Null

    if (-not (Test-Path -LiteralPath $cleanupScript)) {
        throw "Cleanup script is missing: $cleanupScript"
    }

    $bashTestRoot = (& 'C:\msys64\usr\bin\cygpath.exe' -u $testRoot).Trim()
    & 'C:\msys64\usr\bin\bash.exe' $cleanupScript $bashTestRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Cleanup script failed with exit code $LASTEXITCODE"
    }

    if (Get-ChildItem -LiteralPath $testRoot -Directory -Filter 'install-webui-check-*') {
        throw 'A stale install-webui-check directory remains after cleanup.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $testRoot 'keep-this-directory'))) {
        throw 'Cleanup removed an unrelated directory.'
    }

    $fullBuildContents = Get-Content -Raw -LiteralPath $fullBuildScript
    if ($fullBuildContents -notmatch 'install-webui-check-\*') {
        throw 'FULL_BUILD.bat does not clean install-webui-check-* directories.'
    }

    Write-Host 'PASS: stale install-check cleanup is scoped and configured.'
}
finally {
    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

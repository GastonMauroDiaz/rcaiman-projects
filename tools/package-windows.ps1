[CmdletBinding()]
param(
    [string]$BuildDirectory,
    [string]$OutputDirectory,
    [ValidatePattern('^\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?$')]
    [string]$Version = '0.1.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not [Environment]::Is64BitProcess -or $env:OS -ne 'Windows_NT') {
    throw 'Run this script in 64-bit PowerShell on Windows.'
}
$source = Split-Path -Parent $PSScriptRoot
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $source 'build\Desktop_Qt_6_11_1_MinGW_64_bit-Release'
}
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $source 'build\releases' }
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$cache = Get-Content -LiteralPath (Join-Path $build 'CMakeCache.txt') -Raw
function Get-CacheValue([string]$Name) {
    $match = [regex]::Match($cache, '(?m)^' + [regex]::Escape($Name) + ':[^=]+=(.*)$')
    if (-not $match.Success) { throw "Missing CMake cache entry: $Name" }
    return $match.Groups[1].Value.Trim()
}
if ((Get-CacheValue 'CMAKE_BUILD_TYPE') -ne 'Release') { throw 'A Release build is required.' }
if ((Resolve-Path (Get-CacheValue 'CMAKE_HOME_DIRECTORY')).Path -ne $source) {
    throw 'The build directory belongs to a different source checkout.'
}
$about = Get-Content -LiteralPath (Join-Path $source 'assets\branding\about.md') -Raw
if ($about -notmatch ('(?m)^Version ' + [regex]::Escape($Version) + '\s*$')) {
    throw 'The package version must match the version in About.'
}
$readme = Get-Content -LiteralPath (Join-Path $source 'packaging\windows\README.txt') -Raw
if (-not $readme.StartsWith("rcaiman-projects $Version - Windows x64")) {
    throw 'Update the packaged README to match the release version.'
}
$cmake = Get-CacheValue 'CMAKE_COMMAND'
$ctest = Get-CacheValue 'CMAKE_CTEST_COMMAND'
$compilerBin = Split-Path -Parent (Get-CacheValue 'CMAKE_CXX_COMPILER')
$qtRoot = (Resolve-Path (Join-Path (Get-CacheValue 'Qt6_DIR') '..\..\..')).Path
$msysBin = Join-Path (Split-Path -Parent (Split-Path -Parent (Get-CacheValue 'LibRaw_LIBRARY'))) 'bin'
$objdump = Join-Path $compilerBin 'objdump.exe'
$name = "rcaiman-projects-$Version-windows-x64"
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$archive = Join-Path $output "$name.zip"
if ((Test-Path -LiteralPath $archive) -or (Test-Path -LiteralPath "$archive.sha256")) {
    throw 'The package already exists. Choose another output directory; existing releases are not overwritten.'
}
# Every run gets a fresh directory; no old package or user directory is deleted.
$work = Join-Path $output ('.work-' + [guid]::NewGuid().ToString('N'))
$install = Join-Path $work 'install'
$package = Join-Path $work $name
New-Item -ItemType Directory -Path $package -Force | Out-Null

function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable exited with code $LASTEXITCODE" }
}

function Test-PortableProcess([string]$Executable, [switch]$StartupOnly) {
    $info = New-Object System.Diagnostics.ProcessStartInfo
    $info.FileName = $Executable
    $info.WorkingDirectory = $package
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    # Do not allow development installations to satisfy missing package dependencies.
    $info.EnvironmentVariables['PATH'] = [Environment]::SystemDirectory
    foreach ($key in @('QT_PLUGIN_PATH', 'QML2_IMPORT_PATH', 'QML_IMPORT_PATH', 'QT_CONF_PATH')) {
        $info.EnvironmentVariables.Remove($key)
    }
    $info.EnvironmentVariables['QT_QPA_PLATFORM'] = 'offscreen'
    $info.EnvironmentVariables['QT_QPA_PLATFORM_PLUGIN_PATH'] = $probePlugins
    $info.EnvironmentVariables['QT_QPA_FONTDIR'] = Join-Path $env:WINDIR 'Fonts'
    $info.EnvironmentVariables['QT_FORCE_STDERR_LOGGING'] = '1'
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $info
    $started = $false
    try {
        if (-not $process.Start()) { throw "Could not start $Executable" }
        $started = $true
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if ($StartupOnly) {
            if ($process.WaitForExit(2500)) { throw 'The packaged application exited during startup.' }
            $process.Kill()
            $process.WaitForExit()
        } else {
            if (-not $process.WaitForExit(30000)) { throw "Portable test timed out: $Executable" }
            if ($process.ExitCode -ne 0) {
                throw "Portable test failed ($($process.ExitCode)): $Executable`n$($stderr.Result)"
            }
        }
        ($stdout.Result + $stderr.Result) | Set-Content -Encoding UTF8 -LiteralPath (
            Join-Path $work ((Split-Path -Leaf $Executable) + '.log'))
    } finally {
        if ($started -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}

$oldPath = $env:PATH
try {
    $env:PATH = "$qtRoot\bin;$compilerBin;$msysBin;$oldPath"
    Write-Host 'Building and testing Release...'
    Invoke-Checked $cmake @('--build', $build, '--parallel', '4')
    Invoke-Checked $ctest @('--test-dir', $build, '--output-on-failure')
    Write-Host 'Installing the application and its dependencies...'
    $savedErrorPreference = $ErrorActionPreference
    try {
        # Windows PowerShell wraps native stderr as ErrorRecords, even for warnings.
        $ErrorActionPreference = 'Continue'
        $installLog = @(& $cmake --install $build --prefix $install --config Release 2>&1)
        $installExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $savedErrorPreference }
    $installLog | Set-Content -LiteralPath (Join-Path $work 'install.log') -Encoding UTF8
    if ($installExit -ne 0) {
        $installLog | Select-Object -Last 20 | ForEach-Object { Write-Host "$_" }
        throw "Deployment failed. See $work\install.log"
    }

    # Keep CMake's normal installation layout intact; flatten only this portable copy.
    $binName = Get-CacheValue 'CMAKE_INSTALL_BINDIR'
    foreach ($entry in Get-ChildItem -LiteralPath (Join-Path $install $binName) -Force) {
        Copy-Item -LiteralPath $entry.FullName -Destination $package -Recurse
    }
    foreach ($entry in Get-ChildItem -LiteralPath $install -Force) {
        if ($entry.Name -ne $binName) {
            Copy-Item -LiteralPath $entry.FullName -Destination $package -Recurse
        }
    }
    Copy-Item -LiteralPath (Join-Path $source 'packaging\windows\qt.conf') -Destination $package
    Copy-Item -LiteralPath (Join-Path $source 'packaging\windows\README.txt') -Destination $package
    foreach ($required in @('rcaiman-projects.exe', 'plugins\platforms\qwindows.dll',
                           'tools\exiftool\exiftool.exe', 'tools\exiftool\exiftool_files\perl532.dll',
                           'COPYING', 'THIRD_PARTY_NOTICES.md', 'licenses', 'examples')) {
        if (-not (Test-Path -LiteralPath (Join-Path $package $required))) {
            throw "The package is missing $required"
        }
    }

    Write-Host 'Checking all executable and DLL dependencies...'
    $exifRuntime = Join-Path $package 'tools\exiftool\exiftool_files'
    $binaries = Get-ChildItem -LiteralPath $package -Recurse -File |
        Where-Object { $_.Extension -in @('.exe', '.dll') }
    foreach ($binary in $binaries) {
        $headers = @(& $objdump -p $binary.FullName 2>&1)
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $($binary.FullName)" }
        if ($binary.Name -eq 'rcaiman-projects.exe' -and
            -not ($headers -match 'file format pei-x86-64')) {
            throw 'The application is not a Windows x64 executable.'
        }
        foreach ($line in $headers) {
            if ("$line" -match 'DLL Name:\s*(\S+)') {
                $dll = $Matches[1]
                if ($dll -match '^(api-ms-win-|ext-ms-win-)') { continue }
                $found = $false
                # Perl extensions resolve against perl.exe's directory, not the GUI executable.
                $processRoot = $package
                if ($binary.FullName.StartsWith($exifRuntime + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    $processRoot = $exifRuntime
                }
                foreach ($directory in @($binary.DirectoryName, $processRoot, $package, [Environment]::SystemDirectory)) {
                    if (Test-Path -LiteralPath (Join-Path $directory $dll)) { $found = $true; break }
                }
                if (-not $found) { throw "$($binary.Name) requires missing dependency $dll" }
            }
        }
    }

    # The offscreen plugin is a test aid outside the package, not a development PATH entry.
    $probePlugins = Join-Path $work 'probe-platforms'
    New-Item -ItemType Directory -Path $probePlugins | Out-Null
    Copy-Item -LiteralPath (Join-Path $qtRoot 'plugins\platforms\qoffscreen.dll') -Destination $probePlugins
    Write-Host 'Running portable tests with only Windows directories on PATH...'
    foreach ($test in @('metadataregistry_tests.exe', 'thirdpartylicenses_tests.exe', 'imageviewer_measurement_tests.exe')) {
        $probe = Join-Path $package $test
        if (Test-Path -LiteralPath $probe) { throw "Unexpected test executable in package: $test" }
        Copy-Item -LiteralPath (Join-Path $build $test) -Destination $probe
        try { Test-PortableProcess $probe } finally { Remove-Item -LiteralPath $probe }
    }
    Test-PortableProcess (Join-Path $package 'rcaiman-projects.exe') -StartupOnly

    $revision = & git -c "safe.directory=$($source.Replace('\','/'))" -C $source rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the source revision.' }
    $changes = @(& git -c "safe.directory=$($source.Replace('\','/'))" -C $source status --porcelain)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify source changes.' }
    $qtVersion = & (Join-Path $qtRoot 'bin\qmake.exe') -query QT_VERSION
    if ($LASTEXITCODE -ne 0) { throw 'Cannot identify the Qt version.' }
    [ordered]@{
        application = 'rcaiman-projects'; version = $Version; platform = 'windows-x64'
        source_commit = "$revision"; source_has_uncommitted_changes = ($changes.Count -ne 0)
        qt_version = "$qtVersion"; built_at_utc = [DateTime]::UtcNow.ToString('o')
        validation = @('CTest', 'DLL import closure', 'portable metadata/TIFF/project tests',
                       'portable license navigation', 'portable measurement tests', 'headless application startup')
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $package 'BUILD-INFO.json') -Encoding UTF8
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::Open($archive, [IO.Compression.ZipArchiveMode]::Create)
    try {
        # .NET Framework otherwise writes Windows separators into ZIP entry names.
        foreach ($file in Get-ChildItem -LiteralPath $package -Recurse -File -Force) {
            $relative = $file.FullName.Substring($package.Length + 1).Replace('\', '/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName,
                "$name/$relative", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
        foreach ($directory in Get-ChildItem -LiteralPath $package -Recurse -Directory -Force) {
            if (-not (Get-ChildItem -LiteralPath $directory.FullName -Force | Select-Object -First 1)) {
                $relative = $directory.FullName.Substring($package.Length + 1).Replace('\', '/')
                $zip.CreateEntry("$name/$relative/") | Out-Null
            }
        }
    } finally { $zip.Dispose() }
    $zip = [IO.Compression.ZipFile]::OpenRead($archive)
    try {
        if (-not $zip.GetEntry("$name/rcaiman-projects.exe") -or
            -not $zip.GetEntry("$name/BUILD-INFO.json")) {
            throw 'The ZIP does not contain the expected portable layout.'
        }
    } finally { $zip.Dispose() }
    $hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name.zip" | Set-Content -LiteralPath "$archive.sha256" -Encoding ASCII
    Write-Host "Package: $archive"
    Write-Host "SHA256: $hash"
    Write-Host "Validation logs: $work"
} finally {
    $env:PATH = $oldPath
}

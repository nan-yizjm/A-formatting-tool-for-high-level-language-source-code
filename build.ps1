$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourceRoot = Join-Path $projectRoot '实验源程序'
$compiler = Get-Command g++ -ErrorAction Stop
$temporaryExe = Join-Path $sourceRoot 'formatter_build.exe'
$finalExe = Join-Path $sourceRoot 'formatter.exe'

$sources = @(
    (Join-Path $sourceRoot 'main.cpp'),
    (Join-Path $sourceRoot 'source_lexer.cpp'),
    (Join-Path $sourceRoot 'source_parser.cpp'),
    (Join-Path $sourceRoot 'source_formatter.cpp')
)
$flags = @(
    '-std=c++17', '-Wall', '-Wextra', '-pedantic',
    '-finput-charset=UTF-8', '-fexec-charset=UTF-8'
)

Write-Host '[1/2] 正在编译...' -ForegroundColor Cyan
& $compiler.Source @flags @sources '-o' $temporaryExe
if ($LASTEXITCODE -ne 0) {
    throw "编译失败，g++退出码为 $LASTEXITCODE"
}

Move-Item -LiteralPath $temporaryExe -Destination $finalExe -Force
Write-Host '[2/2] 编译成功' -ForegroundColor Green
Write-Host "可执行文件: $finalExe"

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
& (Join-Path $projectRoot 'build.ps1')

$compiler = Get-Command g++ -ErrorAction Stop
$cCompiler = Get-Command gcc -ErrorAction Stop
$sourceRoot = Join-Path $projectRoot '实验源程序'
$testRoot = Join-Path $projectRoot '测试用例'
$runRoot = Join-Path $env:TEMP ("formatter-regression-" + $PID)
New-Item -ItemType Directory -Force -Path $runRoot | Out-Null
$testExe = Join-Path $runRoot 'regression_tests.exe'

Write-Host '[测试] 编译自动测试程序...' -ForegroundColor Cyan
& $compiler.Source '-std=c++17' '-Wall' '-Wextra' '-pedantic' `
    '-finput-charset=UTF-8' '-fexec-charset=UTF-8' `
    ("-I" + $sourceRoot) `
    (Join-Path $testRoot 'regression_tests.cpp') `
    (Join-Path $sourceRoot 'source_lexer.cpp') `
    (Join-Path $sourceRoot 'source_parser.cpp') `
    (Join-Path $sourceRoot 'source_formatter.cpp') `
    '-o' $testExe
if ($LASTEXITCODE -ne 0) { throw '自动测试程序编译失败' }

Push-Location $runRoot
try {
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw '自动回归测试失败' }

    Write-Host '[测试] 使用真实C编译器检查格式化结果...' -ForegroundColor Cyan
    & $cCompiler.Source '-std=c11' '-Wall' '-Wextra' '-pedantic' `
        '-fsyntax-only' (Join-Path $runRoot 'formatted_smoke.c')
    if ($LASTEXITCODE -ne 0) { throw '格式化后的C代码未通过语法编译' }
}
finally {
    Pop-Location
}

Write-Host '所有检查通过。' -ForegroundColor Green

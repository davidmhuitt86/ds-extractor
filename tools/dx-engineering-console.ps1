# EKE-DX-WIRE Engineering Console
# Safe local validation GUI. No Git reset, branch switching, commit, push, or publication.
# Launch: powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\dx-engineering-console.ps1
[CmdletBinding()]
param([string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot), [string]$RunMode = '')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = [IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path (Join-Path $root '.git'))) { throw "Not a Git repository: $root" }
$logDir = Join-Path $root 'artifacts\engineering-console'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null

# Child-process mode is used by the GUI to run checks without blocking the window.
if ($RunMode) {
    $failed = $false
    function Run-Check([string]$Name, [string]$Exe, [string[]]$Args) {
        Write-Output ""; Write-Output "===== $Name ====="
        Write-Output ("COMMAND: {0} {1}" -f $Exe, ($Args -join ' '))
        & $Exe @Args 2>&1 | ForEach-Object { Write-Output $_.ToString() }
        $code = $LASTEXITCODE
        if ($code -ne 0) { $script:failed = $true; Write-Output "RESULT: FAIL (exit $code)" }
        else { Write-Output "RESULT: PASS" }
    }
    $build = Join-Path $root 'build'
    if ($RunMode -in @('ALL','BUILD')) {
        if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) { Run-Check 'Configure CMake' 'cmake.exe' @('-S',$root,'-B',$build) }
        if (-not $script:failed) { Run-Check 'Release build' 'cmake.exe' @('--build',$build,'--config','Release','--parallel','1') }
    }
    if ($RunMode -in @('ALL','TEST')) {
        if (-not (Test-Path (Join-Path $build 'CMakeCache.txt'))) { Run-Check 'Configure CMake' 'cmake.exe' @('-S',$root,'-B',$build) }
        if (-not $script:failed) { Run-Check 'CTest Release suite' 'ctest.exe' @('--test-dir',$build,'-C','Release','--output-on-failure') }
    }
    if ($RunMode -in @('ALL','ARTIFACTS')) {
        Write-Output ""; Write-Output '===== Review artifact JSON validation ====='
        $py = if (Get-Command py.exe -ErrorAction SilentlyContinue) { 'py.exe' } else { 'python.exe' }
        $code = 'import json,sys; from pathlib import Path; r=Path(sys.argv[1]); fs=[r/"artifacts/extraction_review/review_manifest.json",r/"artifacts/audit/extraction_audit.json"]; bad=False; exec("for p in fs:\\n if not p.is_file(): print(\\"FAIL: missing \\"+str(p)); bad=True; continue\\n try:\\n  d=json.loads(p.read_text(encoding=\\"utf-8-sig\\")); print(\\"VALID JSON: \\"+str(p));\\n  if not isinstance(d,dict): print(\\"FAIL: top-level JSON is not an object\\"); bad=True\\n except Exception as e: print(\\"FAIL: \\"+str(p)+\\": \\"+str(e)); bad=True"); sys.exit(1 if bad else 0)'
        & $py -c $code $root 2>&1 | ForEach-Object { Write-Output $_.ToString() }
        if ($LASTEXITCODE -ne 0) { $script:failed = $true }
    }
    if ($RunMode -in @('ALL','GUARD')) {
        $guard = Join-Path $root 'tools\tests\test-dx-publish-review-integrity.ps1'
        if (Test-Path $guard) { Run-Check 'Publication guard regression tests' (Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe') @('-NoProfile','-ExecutionPolicy','Bypass','-File',$guard) }
        else { Write-Output "FAIL: missing guard test script: $guard"; $script:failed = $true }
    }
    if ($script:failed) { Write-Output ""; Write-Output 'OVERALL: FAIL'; exit 1 }
    Write-Output ""; Write-Output 'OVERALL: PASS'; exit 0
}

# GUI mode
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$form = New-Object Windows.Forms.Form
$form.Text = 'EKE-DX-WIRE Engineering Console'
$form.StartPosition = 'CenterScreen'
$form.Size = New-Object Drawing.Size(1050,720)
$form.MinimumSize = New-Object Drawing.Size(850,580)
$form.Font = New-Object Drawing.Font('Segoe UI',9)
$top = New-Object Windows.Forms.Panel; $top.Dock = 'Top'; $top.Height = 85; $top.BackColor = [Drawing.Color]::FromArgb(29,42,61); $form.Controls.Add($top)
$title = New-Object Windows.Forms.Label; $title.Text = 'DX-EXTRACTOR ENGINEERING CONSOLE'; $title.ForeColor = [Drawing.Color]::White; $title.Font = New-Object Drawing.Font('Segoe UI Semibold',15); $title.Location = New-Object Drawing.Point(18,12); $title.AutoSize = $true; $top.Controls.Add($title)
$repoLabel = New-Object Windows.Forms.Label; $repoLabel.Text = "Repository: $root"; $repoLabel.ForeColor = [Drawing.Color]::White; $repoLabel.Location = New-Object Drawing.Point(20,50); $repoLabel.AutoSize = $true; $top.Controls.Add($repoLabel)
$bar = New-Object Windows.Forms.FlowLayoutPanel; $bar.Dock = 'Top'; $bar.Height = 58; $bar.Padding = New-Object Windows.Forms.Padding(8); $form.Controls.Add($bar)
function Button([string]$text,[int]$width=145) { $b=New-Object Windows.Forms.Button; $b.Text=$text; $b.Width=$width; $b.Height=32; $b.Margin=New-Object Windows.Forms.Padding(4); return $b }
$all=Button 'RUN ALL CHECKS' 160; $buildBtn=Button 'BUILD' 90; $testBtn=Button 'RUN TESTS' 110; $art=Button 'VALIDATE ARTIFACTS' 155; $guardBtn=Button 'TEST GUARD' 110; $open=Button 'OPEN REPORTS' 125
$bar.Controls.AddRange(@($all,$buildBtn,$testBtn,$art,$guardBtn,$open))
$status=New-Object Windows.Forms.Label; $status.Dock='Top'; $status.Height=32; $status.Padding=New-Object Windows.Forms.Padding(12,7,0,0); $status.Text='READY — no checks running'; $form.Controls.Add($status)
$live=New-Object Windows.Forms.RichTextBox; $live.Dock='Fill'; $live.ReadOnly=$true; $live.WordWrap=$false; $live.Font=New-Object Drawing.Font('Consolas',9); $live.BackColor=[Drawing.Color]::FromArgb(24,30,38); $live.ForeColor=[Drawing.Color]::FromArgb(220,230,240); $form.Controls.Add($live)
$timer=New-Object Windows.Forms.Timer; $timer.Interval=500
$script:child=$null; $script:stdout=''; $script:stderr=''; $script:shownOut=0; $script:shownErr=0; $script:log=''; $script:report=''
$timer.Add_Tick({
    if ($script:stdout -and (Test-Path $script:stdout)) { $t=Get-Content -LiteralPath $script:stdout -Raw -ErrorAction SilentlyContinue; if ($t.Length -gt $script:shownOut) { $live.AppendText($t.Substring($script:shownOut)); $script:shownOut=$t.Length; $live.SelectionStart=$live.TextLength; $live.ScrollToCaret() } }
    if ($script:stderr -and (Test-Path $script:stderr)) { $t=Get-Content -LiteralPath $script:stderr -Raw -ErrorAction SilentlyContinue; if ($t.Length -gt $script:shownErr) { $live.AppendText($t.Substring($script:shownErr)); $script:shownErr=$t.Length; $live.SelectionStart=$live.TextLength; $live.ScrollToCaret() } }
    if ($script:child -and $script:child.HasExited) {
        $timer.Stop(); $code=$script:child.ExitCode; $script:child.Dispose(); $script:child=$null
        $status.Text=if ($code -eq 0) { 'PASS — run completed' } else { "FAIL — exit code $code; see report" }
        $all.Enabled=$true; $buildBtn.Enabled=$true; $testBtn.Enabled=$true; $art.Enabled=$true; $guardBtn.Enabled=$true
        $reportData=[ordered]@{schema_version=1;finished_at=(Get-Date).ToString('o');repository=$root;branch=((git -C $root rev-parse --abbrev-ref HEAD 2>$null | Out-String).Trim());commit=((git -C $root rev-parse HEAD 2>$null | Out-String).Trim());mode=$script:mode;exit_code=$code;log=$script:log;stdout=$script:stdout;stderr=$script:stderr}
        $reportData | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $script:report -Encoding UTF8
        $live.AppendText("`r`nREPORT: $script:report`r`n")
    }
})
$script:mode=''
function Start-Checks([string]$mode) {
    if ($script:child) { return }
    $script:mode=$mode; $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'; $script:log=Join-Path $logDir "run-$($mode.ToLower())-$stamp.log"; $script:stdout=Join-Path $logDir "stdout-$stamp.log"; $script:stderr=Join-Path $logDir "stderr-$stamp.log"; $script:report=Join-Path $logDir "report-$stamp.json"; $script:shownOut=0; $script:shownErr=0
    Set-Content -LiteralPath $script:log -Value "Mode: $mode`r`nRepository: $root" -Encoding UTF8
    $live.Clear(); $live.AppendText("Starting $mode...`r`nRepository: $root`r`nLogs: $logDir`r`n`r`n")
    $status.Text="RUNNING — $mode"; foreach($b in @($all,$buildBtn,$testBtn,$art,$guardBtn)){$b.Enabled=$false}
    $ps=Join-Path $env:WINDIR 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,'-RepositoryRoot',$root,'-RunMode',$mode)
    $script:child=Start-Process -FilePath $ps -ArgumentList $args -WorkingDirectory $root -PassThru -RedirectStandardOutput $script:stdout -RedirectStandardError $script:stderr
    $timer.Start()
}
$all.Add_Click({Start-Checks 'ALL'})
$buildBtn.Add_Click({Start-Checks 'BUILD'})
$testBtn.Add_Click({Start-Checks 'TEST'})
$art.Add_Click({Start-Checks 'ARTIFACTS'})
$guardBtn.Add_Click({Start-Checks 'GUARD'})
$open.Add_Click({Start-Process explorer.exe -ArgumentList "`"$logDir`""})
$form.Add_FormClosing({ if ($script:child -and -not $script:child.HasExited) { $answer=[Windows.Forms.MessageBox]::Show('Checks are still running. Close the console and stop the child process?','Run in progress','YesNo','Warning'); if($answer -ne [Windows.Forms.DialogResult]::Yes){$_.Cancel=$true}else{try{$script:child.Kill()}catch{}} } })
[void]$form.ShowDialog()
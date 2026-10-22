$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
$installer=Get-Content (Join-Path $repoRoot 'scripts/CommentarySdInstaller.cs') -Raw
$tests=Get-Content (Join-Path $PSScriptRoot 'commentary-installer.test.cs') -Raw
$tests=[regex]::Replace($tests,'(?m)^using [^\r\n]+;\r?\n','')
# Compila contra System.IO.Ports por la clase de producción; las pruebas inyectan
# exclusivamente CommentaryFakeTransport. Ni siquiera se construye un SerialPort.
Add-Type -TypeDefinition ($installer+"`n"+$tests) -ReferencedAssemblies (@((Get-ChildItem (Join-Path $PSHOME 'ref/*.dll')).FullName) + @([IO.Ports.SerialPort].Assembly.Location))
[CommentaryInstallerTests]::Run()

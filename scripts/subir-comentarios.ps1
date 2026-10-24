param([string]$Package, [string]$Port='COM6', [switch]$Install, [switch]$CompileOnly)
$ErrorActionPreference='Stop'
Add-Type -TypeDefinition (Get-Content (Join-Path $PSScriptRoot 'CommentarySdInstaller.cs') -Raw) -ReferencedAssemblies (@((Get-ChildItem (Join-Path $PSHOME 'ref\*.dll')).FullName) + @([IO.Ports.SerialPort].Assembly.Location))
if($CompileOnly){Write-Output 'Instalador compilado, sin abrir puertos';return}
if(-not $Install){throw 'Se necesita -Install y autorización del dueño antes de abrir el puerto y escribir la SD.'}
$root=(Resolve-Path -LiteralPath $Package).Path
$m=Get-Content (Join-Path $root 'catalogo.json') -Raw | ConvertFrom-Json
if($m.schema -ne 2 -or $m.files.Count -lt 1 -or $m.files.Count -gt 1024){throw 'Catálogo inválido'}
if($m.catalog_crc32 -notmatch '^[0-9a-f]{8}$'){throw 'CRC del catálogo inválido'}
$minimum=[string]$m.minimum_firmware
if($minimum -notmatch '^[0-9]+(?:\.[0-9]+){1,3}$'){throw 'Falta minimum_firmware válido en el catálogo'}
# El instalador exige además >=0.6 por SONIDO_INFO. upload_verified es solo histórico.
$allFiles=@($m.files)
foreach($f in $m.files){if($f.pool -notin @(0..38)){throw 'Grupo inválido'}}
if($m.PSObject.Properties.Name -contains 'reserved_files'){
  foreach($f in $m.reserved_files){if($null -ne $f.pool){throw 'Un reservado no debe tener pool activo'}}
  $allFiles+=@($m.reserved_files)
}
if($allFiles.Count -gt 1024){throw 'Demasiados sonidos en el paquete'}
$paths=@();$codes=@();$crcs=@();$hashes=@();$verified=@()
foreach($f in $allFiles){
  if($f.code -notmatch '^[0-9a-f]{8}$' -or $f.crc32 -notmatch '^[0-9a-f]{8}$' -or $f.sha256 -notmatch '^[0-9a-f]{64}$'){throw 'Hashes inválidos'}
  $resolved=[IO.Path]::GetFullPath((Join-Path $root $f.package_path))
  if(-not $resolved.StartsWith($root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Ruta fuera del paquete'}
  $paths+=$resolved;$codes+=$f.code;$crcs+=$f.crc32;$hashes+=$f.sha256;$verified+=[bool]$f.upload_verified
}
$commands=@('COMENTARIOS_INICIO '+$m.files.Count)
foreach($f in $m.files){$commands+=('COMENTARIOS_ITEM '+$f.pool+' '+$f.code)}
$commands+=('COMENTARIOS_FIN '+$m.catalog_crc32)
[CommentarySdInstaller]::new().Run($Port,$root,[string[]]$paths,[string[]]$codes,[string[]]$crcs,[string[]]$hashes,[bool[]]$verified,[string[]]$commands,$minimum)

// Instalador USB de datos. No reproduce voces ni inicia partidos.
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Text.RegularExpressions;
using System.Security.Cryptography;

public interface ICommentaryTransport : IDisposable {
  bool IsOpen { get; }
  void Open();
  void Close();
  void Write(string value);
  string ReadLine(); // TimeoutException sin una línea completa.
}

public sealed class CommentarySerialTransport : ICommentaryTransport {
  readonly SerialPort serial;
  public CommentarySerialTransport(string port) {
    serial=new SerialPort(port,115200) { DtrEnable=false,RtsEnable=false,ReadBufferSize=65536,ReadTimeout=250,WriteTimeout=3000 };
  }
  public bool IsOpen { get { return serial.IsOpen; } }
  public void Open(){serial.Open();}
  public void Close(){serial.Close();}
  public void Write(string value){serial.Write(value);}
  public string ReadLine(){return serial.ReadLine();}
  public void Dispose(){serial.Dispose();}
}

public sealed class CommentaryProtocolException : Exception {
  public CommentaryProtocolException(string value):base(value){}
}

public sealed class CommentarySdInstaller {
  readonly Func<string,ICommentaryTransport> createTransport;
  readonly Func<long> milliseconds;
  ICommentaryTransport connection;
  string log;
  readonly List<string> pendingLog=new List<string>();
  public int BootWaitMilliseconds {get;set;}=6000;
  const ulong ClusterAllowance=65536; // Reserva conservadora, no tamaño de cluster medido.
  const ulong BaseReserve=1048576;

  public CommentarySdInstaller():this(port=>new CommentarySerialTransport(port),()=>Environment.TickCount64){}
  public CommentarySdInstaller(Func<string,ICommentaryTransport> transport,Func<long> clock) {
    createTransport=transport??throw new ArgumentNullException(nameof(transport));
    milliseconds=clock??throw new ArgumentNullException(nameof(clock));
  }
  void Record(string value) {
    string line=DateTime.UtcNow.ToString("o")+" "+value+Environment.NewLine;
    if(log==null)pendingLog.Add(line);else File.AppendAllText(log,line);
  }
  void BindJournal(string package,string board,string volume) {
    // El diario no demuestra integridad ni identifica inequívocamente una SD sin CID.
    log=Path.Combine(package,"instalacion-comentarios-"+board+"-"+volume+".log");
    foreach(string line in pendingLog)File.AppendAllText(log,line);
    pendingLog.Clear();
  }
  string Exchange(string command,string prefix,int seconds=8) {
    try{connection.Write(command+"\n");}
    catch(TimeoutException error){throw new IOException("Envio USB incompleto; cerrar y repetir la instalacion para recuperar de forma segura",error);}
    long start=milliseconds();
    while(milliseconds()-start<seconds*1000L) {
      try {
        string line=connection.ReadLine().TrimEnd('\r');
        if(line.StartsWith("SONIDO_ERROR ") || line.StartsWith("SONIDOS_ERROR ") || line.StartsWith("COMENTARIOS_ERROR "))
          throw new CommentaryProtocolException(line);
        if(line.StartsWith(prefix,StringComparison.Ordinal))return line;
        Record("INFO "+line);
      }catch(TimeoutException){}
    }
    throw new TimeoutException("Sin respuesta a "+command.Split(' ')[0]);
  }
  static bool Recoverable(Exception error) {
    return error is TimeoutException || (error is CommentaryProtocolException && error.Message.EndsWith(" tiempo",StringComparison.Ordinal));
  }
  public static uint Crc(byte[] data) {
    uint crc=0xffffffff;foreach(byte b in data){crc^=b;for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1)!=0?0xedb88320:0);}
    return crc^0xffffffff;
  }
  static Version VersionOf(string value) {
    if(value==null || !Regex.IsMatch(value,@"^[0-9]+(?:\.[0-9]+){1,3}$"))throw new Exception("Version de firmware invalida");
    var parts=value.Split('.').Select(int.Parse).ToArray();
    return new Version(parts[0],parts[1],parts.Length>2?parts[2]:0,parts.Length>3?parts[3]:0);
  }
  static byte[] LocalFile(string path,string crc,string hash) {
    var info=new FileInfo(path);
    if(info.Length<1 || info.Length>1048576)throw new Exception("Tamano local invalido: "+Path.GetFileName(path));
    byte[] data=File.ReadAllBytes(path);
    if(data.Length<1 || data.Length>1048576 || Crc(data).ToString("x8")!=crc || Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant()!=hash)
      throw new Exception("Archivo local alterado: "+Path.GetFileName(path));
    return data;
  }
  struct RemoteFile {public bool Exists;public ulong Bytes;public string Crc;}
  RemoteFile Inspect(string code) {
    string result=Exchange("SONIDO_INFO "+code,"SONIDO_INFO "+code+" ",60);
    Record("REMOTE "+result);
    if(result=="SONIDO_INFO "+code+" ausente=1")return new RemoteFile();
    var m=Regex.Match(result,"^SONIDO_INFO "+code+@" tam=([0-9]+) crc32=([0-9a-fA-F]{8})$");
    ulong bytes;
    if(!m.Success || !ulong.TryParse(m.Groups[1].Value,out bytes))throw new Exception("Respuesta de checksum incorrecta: "+code);
    return new RemoteFile {Exists=true,Bytes=bytes,Crc=m.Groups[2].Value.ToLowerInvariant()};
  }
  static bool Matches(RemoteFile remote,long size,string crc){return remote.Exists && remote.Bytes==(ulong)size && remote.Crc==crc;}
  static void RefuseDifferent(RemoteFile remote,long size,string crc,string code) {
    if(remote.Exists && !Matches(remote,size,crc))throw new Exception("Archivo SD diferente: "+code+". Se necesita una sustitucion explicita; no se borrara.");
  }
  ulong IdleSpace(out ulong total) {
    total=0;
    string info=Exchange("SDINFO?","SDINFO ");Record(info);
    var m=Regex.Match(info,@"^SDINFO disponible=1 partido=0 total=([0-9]+) usado=([0-9]+) libre=([0-9]+)(?: .*)?$");
    ulong used,free;
    if(!m.Success || !ulong.TryParse(m.Groups[1].Value,out total) || !ulong.TryParse(m.Groups[2].Value,out used) || !ulong.TryParse(m.Groups[3].Value,out free) || total==0 || free>total || used!=total-free)
      throw new Exception("SD no disponible, hay partido activo o capacidad incoherente");
    return free;
  }
  static long[] ValidatePackage(string[] paths,string[] codes,string[] crcs,string[] hashes,string[] commands,out int activeCount) {
    activeCount=0;int count=paths==null?0:paths.Length;
    if(count<1 || count>1024 || codes==null || crcs==null || hashes==null || codes.Length!=count || crcs.Length!=count || hashes.Length!=count || commands==null || commands.Length<3 || commands.Length>count+2)
      throw new Exception("Paquete de comentarios invalido");
    var header=Regex.Match(commands[0]??"","^COMENTARIOS_INICIO ([0-9]+)$");
    if(!header.Success || !int.TryParse(header.Groups[1].Value,out activeCount) || activeCount<1 || activeCount>count || commands.Length!=activeCount+2)
      throw new Exception("Cantidad de catalogo incorrecta");
    var unique=new HashSet<string>();var sizes=new long[count];
    for(int i=0;i<count;i++) {
      if(codes[i]==null || !Regex.IsMatch(codes[i],"^[0-9a-f]{8}$") || codes[i]=="00000000" || !unique.Add(codes[i]) || crcs[i]==null || !Regex.IsMatch(crcs[i],"^[0-9a-f]{8}$") || hashes[i]==null || !Regex.IsMatch(hashes[i],"^[0-9a-f]{64}$"))
        throw new Exception("Codigo, duplicado o hash invalido");
      sizes[i]=LocalFile(paths[i],crcs[i],hashes[i]).LongLength;
    }
    // Archivos reservados también se verifican/suben, pero nunca se añaden al catálogo activo.
    var rows=new byte[activeCount*5];var active=new HashSet<string>();int previousPool=-1;
    for(int i=0;i<activeCount;i++) {
      var item=Regex.Match(commands[i+1]??"","^COMENTARIOS_ITEM ([0-9]|[12][0-9]|30) ([0-9a-f]{8})$");
      if(!item.Success || !unique.Contains(item.Groups[2].Value) || !active.Add(item.Groups[2].Value))throw new Exception("Fila de catalogo incorrecta");
      int pool=int.Parse(item.Groups[1].Value);if(pool<previousPool)throw new Exception("Catalogo no ordenado por grupo");previousPool=pool;
      rows[i*5]=(byte)pool;uint code=Convert.ToUInt32(item.Groups[2].Value,16);for(int j=0;j<4;j++)rows[i*5+j+1]=(byte)(code>>(j*8));
    }
    if(commands[activeCount+1]!="COMENTARIOS_FIN "+Crc(rows).ToString("x8"))throw new Exception("CRC del catalogo incorrecta");
    return sizes;
  }
  void Transfer(string path,string code,string crc,string hash,long size,ulong expectedTotal) {
    byte[] data=LocalFile(path,crc,hash); // También protege cambios locales después del preflight.
    for(int attempt=1;attempt<=3;attempt++) {
      ulong total;IdleSpace(out total);if(total!=expectedTotal)throw new Exception("Cambio de volumen SD durante la instalacion");
      string stage="inicio";
      Record("INTENT "+code+" bytes="+size+" crc32="+crc+" sha256="+hash+" intento="+attempt);
      try {
        // Esperar más que los 8 s del receptor permite consumir su aviso de timeout
        // antes de iniciar el siguiente intento, en vez de leerlo como error atrasado.
        string ready=Exchange("SONIDO_SUBIR sd "+code+" "+data.Length,"SONIDO_LISTO ",12);
        if(ready!="SONIDO_LISTO "+code)throw new Exception("Confirmacion de inicio incorrecta");
        stage="datos";
        for(int at=0;at<data.Length;at+=192) {
          int length=Math.Min(192,data.Length-at);
          string ack=Exchange("SONIDO_DATOS "+Convert.ToBase64String(data,at,length),"SONIDO_OK ",12);
          if(ack!="SONIDO_OK "+(at+length))throw new Exception("Desplazamiento de recepcion incorrecto: "+code);
        }
        stage="fin";
        // FIN verifica la pista completa en SD y puede tardar en una tarjeta lenta.
        string done=Exchange("SONIDO_FIN "+crc,"SONIDO_GUARDADO ",60);
        if(done!="SONIDO_GUARDADO "+code)throw new Exception("Confirmacion final incorrecta");
      }catch(Exception error) {
        if(!Recoverable(error))throw;
        Record("UNCERTAIN "+code+" etapa="+stage+" "+error.Message);
        if(stage=="fin") {
          // FIN puede haberse confirmado en SD aunque su ACK se haya perdido.
          var remote=Inspect(code);RefuseDifferent(remote,size,crc,code);
          if(Matches(remote,size,crc)){Record("RECOVERED_FIN "+code);return;}
        }
        if(attempt==3)throw;
        // Nunca reenviar el bloque: SONIDO_SUBIR cancela el temporal y empieza desde cero.
        Record("RETRY_WHOLE_FILE "+code);continue;
      }
      var stored=Inspect(code);
      if(!Matches(stored,size,crc))throw new Exception("El archivo escrito no coincide al releerlo: "+code);
      return;
    }
  }
  public void Run(string port,string package,string[] paths,string[] codes,string[] crcs,string[] hashes,bool[] verified,string[] catalogCommands,string minimumFirmware="0.6") {
    // verified y los diarios anteriores son solo historial; nunca autorizan saltarse la lectura remota.
    int activeCount;long[] sizes=ValidatePackage(paths,codes,crcs,hashes,catalogCommands,out activeCount);
    Version minimum=VersionOf(minimumFirmware),protocolMinimum=VersionOf(catalogCommands.Any(c=>Regex.IsMatch(c,"^COMENTARIOS_ITEM (2[4-9]|30) "))?"0.12":catalogCommands.Any(c=>Regex.IsMatch(c,"^COMENTARIOS_ITEM (1[1-9]|2[0-3]) "))?"0.11":catalogCommands.Any(c=>Regex.IsMatch(c,"^COMENTARIOS_ITEM (9|10) "))?"0.10":catalogCommands.Any(c=>Regex.IsMatch(c,"^COMENTARIOS_ITEM [6-8] "))?"0.7":"0.6");if(minimum<protocolMinimum)minimum=protocolMinimum;
    if(!Directory.Exists(package))throw new Exception("Paquete inexistente");
    log=null;pendingLog.Clear();connection=createTransport(port);
    try {
      connection.Open();long boot=milliseconds();
      while(milliseconds()-boot<BootWaitMilliseconds){try{Record("BOOT "+connection.ReadLine().TrimEnd('\r'));}catch(TimeoutException){}}
      string hello=Exchange("HELLO","HELLO ");Record(hello);
      var version=Regex.Match(hello,@"^HELLO waveshare-7c ([0-9]+(?:\.[0-9]+){1,3}) caps=");
      if(!version.Success || VersionOf(version.Groups[1].Value)<minimum)throw new Exception("Se requiere Waveshare 7C con firmware >= "+minimum);
      ulong total,free=IdleSpace(out total);
      string identity=Exchange("SD_IDENTIDAD?","SD_IDENTIDAD ");
      var id=Regex.Match(identity,@"^SD_IDENTIDAD placa=([0-9a-fA-F]{12}) volumen=([0-9]+)$");ulong volume;
      if(!id.Success || !ulong.TryParse(id.Groups[2].Value,out volume) || volume!=total)throw new Exception("Identidad o volumen SD incoherentes");
      BindJournal(package,id.Groups[1].Value.ToLowerInvariant(),id.Groups[2].Value);Record(identity);
      var skip=new bool[codes.Length];ulong needed=0;var buckets=new HashSet<string>();int added=0;
      for(int i=0;i<codes.Length;i++) {
        var remote=Inspect(codes[i]);RefuseDifferent(remote,sizes[i],crcs[i],codes[i]);skip[i]=remote.Exists;
        if(!skip[i]){needed+=(ulong)sizes[i];added++;buckets.Add(codes[i].Substring(0,2));}
      }
      // Sin geometría de cluster: reserva conservadora de 64 KiB por archivo/carpeta nueva,
      // más 1 MiB para catálogo y temporales. No equivale a medir el consumo FAT exacto.
      ulong reserve=BaseReserve+ClusterAllowance*(ulong)(added+buckets.Count);
      Record("SPACE bytes="+needed+" reserva="+reserve+" libre="+free);
      if(free<needed+reserve)throw new Exception("Espacio insuficiente para nuevos archivos y reserva conservadora");
      for(int i=0;i<codes.Length;i++) {
        if(skip[i]){Record("CONSERVADO_VERIFICADO "+codes[i]+" crc32="+crcs[i]);continue;}
        // No sustituir si otro archivo apareció desde el preflight.
        var remote=Inspect(codes[i]);RefuseDifferent(remote,sizes[i],crcs[i],codes[i]);
        if(!remote.Exists)Transfer(paths[i],codes[i],crcs[i],hashes[i],sizes[i],total);
        Record("DONE "+codes[i]+" sha256="+hashes[i]);Console.WriteLine((i+1)+"/"+codes.Length+" verificado "+codes[i]);
      }
      ulong finalTotal;IdleSpace(out finalTotal);if(finalTotal!=total)throw new Exception("Cambio de volumen SD");
      // Verificar todos otra vez, incluidos los que existían. No depende del orden/límite del listado.
      for(int i=0;i<codes.Length;i++)if(!Matches(Inspect(codes[i]),sizes[i],crcs[i]))throw new Exception("Verificacion final fallida: "+codes[i]);
      foreach(string command in catalogCommands) {
        string expected=command.StartsWith("COMENTARIOS_INICIO ")?"COMENTARIOS_LISTO":command.StartsWith("COMENTARIOS_ITEM ")?"COMENTARIOS_ITEM_OK":"COMENTARIOS_GUARDADO";
        string result=Exchange(command,expected,60);if(result!=expected)throw new Exception("Confirmacion de catalogo incorrecta");
      }
      string status=Exchange("COMENTARIOS?","COMENTARIOS ");Record(status);
      if(!Regex.IsMatch(status,"^COMENTARIOS total="+activeCount+@" limite=1024(?: |$)"))throw new Exception("Cantidad activa inesperada");
      Record("CATALOGO_ACTIVO "+activeCount);Record("DATOS_VERIFICADOS total="+codes.Length+" activos="+activeCount+" reservados="+(codes.Length-activeCount));Console.WriteLine("INSTALACION_DATOS_VERIFICADA "+codes.Length+" activos="+activeCount+" reservados="+(codes.Length-activeCount));
    }finally {
      if(connection!=null){if(connection.IsOpen)connection.Close();connection.Dispose();connection=null;}
    }
  }
}

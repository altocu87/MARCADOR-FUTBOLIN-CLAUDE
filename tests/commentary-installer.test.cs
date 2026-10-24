using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;

// Emula respuestas de la placa: no contiene SerialPort ni abre dispositivos.
public sealed class CommentaryFakeTransport : ICommentaryTransport {
  public long Now;
  public bool IsOpen {get;private set;}
  public bool Opened,Disposed,MatchActive,DropDataAckOnce,DropReadyAckOnce,DropFinalAckOnce,ReceiverTimeoutOnce,CorruptCommit,DropInfo,PartialWriteTimeout;
  public int ReceiverTimeoutDeliveryDelayMilliseconds,FinalAckDelayMilliseconds,TimeoutNotifications;
  public string Firmware="0.6",Board="aabbccddeeff";
  public ulong Capacity=8UL<<30,Free=7UL<<30;
  public int UploadStarts,UploadFinishes,CatalogWrites,InfoQueries;
  public readonly Dictionary<string,byte[]> Files=new Dictionary<string,byte[]>();
  public readonly List<string> Commands=new List<string>();
  readonly Queue<string> replies=new Queue<string>();
  MemoryStream incoming;
  string receiving;
  string delayedTimeout,delayedFinalAck;
  long lastDataAt,timeoutReadyAt,finalAckReadyAt;
  int expected,catalogCount;
  public void Open(){IsOpen=true;Opened=true;}
  public void Close(){IsOpen=false;}
  public void Dispose(){Disposed=true;}
  public string ReadLine(){
    // El receptor vence a los 8 s. Su lÃƒÂ­nea puede llegar mÃƒÂ¡s tarde por USB,
    // siempre antes de las respuestas a ÃƒÂ³rdenes posteriores (orden del flujo).
    if(receiving!=null && Now-lastDataAt>=8000){
      delayedTimeout="SONIDO_ERROR "+receiving+" tiempo";receiving=null;
      timeoutReadyAt=lastDataAt+8000+ReceiverTimeoutDeliveryDelayMilliseconds;TimeoutNotifications++;
    }
    if(delayedTimeout!=null){
      if(Now>=timeoutReadyAt){string line=delayedTimeout;delayedTimeout=null;return line;}
      Now+=250;throw new TimeoutException();
    }
    if(delayedFinalAck!=null){
      if(Now>=finalAckReadyAt){string line=delayedFinalAck;delayedFinalAck=null;return line;}
      Now+=250;throw new TimeoutException();
    }
    if(replies.Count>0)return replies.Dequeue();Now+=250;throw new TimeoutException();
  }
  public void Write(string wire){
    string command=wire.TrimEnd('\n');Commands.Add(command);var parts=command.Split(' ');
    switch(parts[0]){
      case "HELLO":replies.Enqueue("HELLO waveshare-7c "+Firmware+" caps=goles,sonido,sd");break;
      case "SDINFO?":replies.Enqueue("SDINFO disponible=1 partido="+(MatchActive?1:0)+" total="+Capacity+" usado="+(Capacity-Free)+" libre="+Free);break;
      case "SD_IDENTIDAD?":replies.Enqueue("SD_IDENTIDAD placa="+Board+" volumen="+Capacity);break;
      case "SONIDO_INFO":
        InfoQueries++;if(DropInfo)break;
        if(Files.TryGetValue(parts[1],out byte[] stored))replies.Enqueue("SONIDO_INFO "+parts[1]+" tam="+stored.Length+" crc32="+CommentarySdInstaller.Crc(stored).ToString("x8"));
        else replies.Enqueue("SONIDO_INFO "+parts[1]+" ausente=1");break;
      case "SONIDO_SUBIR":
        receiving=parts[2];expected=int.Parse(parts[3]);incoming=new MemoryStream();UploadStarts++;lastDataAt=Now;
        if(DropReadyAckOnce){DropReadyAckOnce=false;break;}
        replies.Enqueue("SONIDO_LISTO "+receiving);break;
      case "SONIDO_DATOS":
        if(PartialWriteTimeout){PartialWriteTimeout=false;throw new TimeoutException("envio incompleto");}
        if(ReceiverTimeoutOnce){ReceiverTimeoutOnce=false;replies.Enqueue("SONIDO_ERROR "+receiving+" tiempo");receiving=null;break;}
        if(receiving==null)throw new Exception("El instalador envio datos sin transaccion");
        byte[] chunk=Convert.FromBase64String(parts[1]);incoming.Write(chunk,0,chunk.Length);lastDataAt=Now;
        if(DropDataAckOnce){DropDataAckOnce=false;break;}
        replies.Enqueue("SONIDO_OK "+incoming.Length);break;
      case "SONIDO_FIN":
        byte[] bytes=incoming.ToArray();
        if(bytes.Length!=expected || CommentarySdInstaller.Crc(bytes).ToString("x8")!=parts[1])throw new Exception("CRC/tamano recibido incorrecto en el falso");
        string code=receiving;receiving=null;Files[code]=bytes;UploadFinishes++;
        if(CorruptCommit)Files[code][0]^=255;
        if(DropFinalAckOnce){DropFinalAckOnce=false;break;}
        if(FinalAckDelayMilliseconds>0){delayedFinalAck="SONIDO_GUARDADO "+code;finalAckReadyAt=Now+FinalAckDelayMilliseconds;break;}
        replies.Enqueue("SONIDO_GUARDADO "+code);break;
      case "COMENTARIOS_INICIO":catalogCount=int.Parse(parts[1]);replies.Enqueue("COMENTARIOS_LISTO");break;
      case "COMENTARIOS_ITEM":replies.Enqueue("COMENTARIOS_ITEM_OK");break;
      case "COMENTARIOS_FIN":CatalogWrites++;replies.Enqueue("COMENTARIOS_GUARDADO");break;
      case "COMENTARIOS?":replies.Enqueue("COMENTARIOS total="+catalogCount+" limite=1024 grupos="+catalogCount+",0,0,0,0,0");break;
      default:throw new Exception("Orden inesperada en transporte falso: "+command);
    }
  }
}

public sealed class CommentaryFixture : IDisposable {
  public readonly string Root;
  public readonly string[] Paths,Codes,Crcs,Hashes,Catalog;
  public readonly byte[][] Data;
  public CommentaryFixture(int count=1){
    Root=Path.Combine(Path.GetTempPath(),"mfv3-commentary-test-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(Root);
    Paths=new string[count];Codes=new string[count];Crcs=new string[count];Hashes=new string[count];Data=new byte[count][];
    Catalog=new string[count+2];Catalog[0]="COMENTARIOS_INICIO "+count;byte[] rows=new byte[count*5];
    for(int i=0;i<count;i++){
      Codes[i]=(0x12340001u+(uint)i).ToString("x8");Data[i]=Enumerable.Range(0,400+i*13).Select(x=>(byte)(x*7+i)).ToArray();
      Paths[i]=Path.Combine(Root,Codes[i]+".wav");File.WriteAllBytes(Paths[i],Data[i]);
      Crcs[i]=CommentarySdInstaller.Crc(Data[i]).ToString("x8");Hashes[i]=Convert.ToHexString(SHA256.HashData(Data[i])).ToLowerInvariant();
      Catalog[i+1]="COMENTARIOS_ITEM 0 "+Codes[i];uint code=Convert.ToUInt32(Codes[i],16);for(int j=0;j<4;j++)rows[i*5+j+1]=(byte)(code>>(8*j));
    }
    Catalog[count+1]="COMENTARIOS_FIN "+CommentarySdInstaller.Crc(rows).ToString("x8");
  }
  public void SetPool(byte pool){
    if(Codes.Length!=1)throw new Exception("Fixture de un solo archivo requerida");
    Catalog[1]="COMENTARIOS_ITEM "+pool+" "+Codes[0];byte[] row=new byte[5];row[0]=pool;
    uint code=Convert.ToUInt32(Codes[0],16);for(int j=0;j<4;j++)row[j+1]=(byte)(code>>(8*j));
    Catalog[2]="COMENTARIOS_FIN "+CommentarySdInstaller.Crc(row).ToString("x8");
  }
  public string[] SubsetCatalog(params int[] indexes){
    var commands=new List<string>{"COMENTARIOS_INICIO "+indexes.Length};var rows=new byte[indexes.Length*5];
    for(int i=0;i<indexes.Length;i++){
      string code=Codes[indexes[i]];commands.Add("COMENTARIOS_ITEM 0 "+code);
      uint value=Convert.ToUInt32(code,16);for(int j=0;j<4;j++)rows[i*5+j+1]=(byte)(value>>(8*j));
    }
    commands.Add("COMENTARIOS_FIN "+CommentarySdInstaller.Crc(rows).ToString("x8"));return commands.ToArray();
  }
  public void Install(CommentaryFakeTransport fake,bool verified=false,string minimum="0.6",string[] commands=null){
    var installer=new CommentarySdInstaller(port=>fake,()=>fake.Now){BootWaitMilliseconds=0};
    installer.Run("TRANSPORTE_FALSO",Root,Paths,Codes,Crcs,Hashes,Enumerable.Repeat(verified,Codes.Length).ToArray(),commands??Catalog,minimum);
  }
  public void Dispose(){
    // Solo borra la carpeta temporal aleatoria creada por esta misma fixture.
    string parent=Path.GetFullPath(Path.GetTempPath()).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
    if(!Path.GetFullPath(Root).StartsWith(parent,StringComparison.OrdinalIgnoreCase) || !Path.GetFileName(Root).StartsWith("mfv3-commentary-test-",StringComparison.Ordinal))throw new Exception("Ruta de limpieza no propia");
    Directory.Delete(Root,true);
  }
}

public static class CommentaryInstallerTests {
  static int passed;
  static void Check(bool value,string message){if(!value)throw new Exception(message);}
  static void Throws(Action action,string fragment){Exception error=null;try{action();}catch(Exception e){error=e;}Check(error!=null && error.Message.Contains(fragment),"Se esperaba fallo '"+fragment+"', recibido: "+error);}
  static void Test(string name,Action action){action();passed++;Console.WriteLine("OK "+name);}
  public static void Run(){
    Test("nuevos: hash local y relectura real antes de activar",()=>{using(var f=new CommentaryFixture(2)){var t=new CommentaryFakeTransport();f.Install(t);Check(t.Files.Count==2 && t.CatalogWrites==1,"Falta archivo/catalogo");Check(t.InfoQueries>=8,"No se releen todos los archivos");Check(!t.IsOpen&&t.Disposed,"No se cerro transporte");Check(!t.Commands.Any(c=>c.StartsWith("SONIDOS_PAGINA")),"Dependencia del inventario truncado");}});
    Test("existente correcto se conserva incluso con flag false y sin diario",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport();t.Files[f.Codes[0]]=f.Data[0].ToArray();f.Install(t);Check(t.UploadStarts==0&&t.InfoQueries==2&&t.CatalogWrites==1,"No se conservo/verifico correctamente");}});
    Test("flag true no permite saltarse CRC de otra SD",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport();t.Files[f.Codes[0]]=f.Data[0].ToArray();t.Files[f.Codes[0]][3]^=1;var original=t.Files[f.Codes[0]].ToArray();Throws(()=>f.Install(t,true),"Archivo SD diferente");Check(t.UploadStarts==0&&t.CatalogWrites==0&&t.Files[f.Codes[0]].SequenceEqual(original),"Se modifico archivo diferente");}});
    Test("mismo paquete en otra placa no confia en diario ni flag",()=>{using(var f=new CommentaryFixture()){var a=new CommentaryFakeTransport();f.Install(a);var b=new CommentaryFakeTransport{Board="112233445566"};b.Files[f.Codes[0]]=new byte[]{1,2,3};Throws(()=>f.Install(b,true),"Archivo SD diferente");Check(b.UploadStarts==0,"Sobrescritura cruzada");Check(Directory.GetFiles(f.Root,"instalacion-comentarios-*.log").Length==2,"Diarios no vinculados a placa");}});
    Test("ACK final perdido se resuelve por bytes reales sin nueva subida",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{DropFinalAckOnce=true};f.Install(t);Check(t.UploadStarts==1&&t.UploadFinishes==1&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0])&&t.CatalogWrites==1,"FIN no idempotente");Check(File.ReadAllText(Directory.GetFiles(f.Root,"*.log")[0]).Contains("RECOVERED_FIN"),"Falta evidencia de recuperacion");}});
    Test("ACK datos perdido reinicia el archivo sin duplicar bloque",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{DropDataAckOnce=true};f.Install(t);Check(t.UploadStarts==2&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0])&&t.CatalogWrites==1,"Se duplico el bloque o no se recupero");}});
    Test("aviso de timeout atrasado por USB se consume antes de reanudar",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{DropDataAckOnce=true,ReceiverTimeoutDeliveryDelayMilliseconds=1000};f.Install(t);Check(t.TimeoutNotifications==1&&t.Now>=9000&&t.Now<12000&&t.UploadStarts==2&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0]),"El timeout atrasado aborto el siguiente IdleSpace o duplico datos");}});
    Test("ACK READY perdido con aviso atrasado reinicia transaccion",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{DropReadyAckOnce=true,ReceiverTimeoutDeliveryDelayMilliseconds=1000};f.Install(t);Check(t.TimeoutNotifications==1&&t.UploadStarts==2&&t.UploadFinishes==1&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0]),"No se recupero la preparacion");}});
    Test("FIN lento dispone de60s sin falso timeout de8s",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{FinalAckDelayMilliseconds=45000};f.Install(t);string journal=File.ReadAllText(Directory.GetFiles(f.Root,"*.log")[0]);Check(t.Now>=45000&&t.UploadStarts==1&&t.CatalogWrites==1&&!journal.Contains("UNCERTAIN"),"FIN lento se trato como ACK perdido");}});
    Test("timeout del receptor recupera desde cero",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{ReceiverTimeoutOnce=true};f.Install(t);Check(t.UploadStarts==2&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0]),"No se recupero timeout real");}});
    Test("escritura USB parcial se detiene sin reintento ciego",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{PartialWriteTimeout=true};Throws(()=>f.Install(t),"Envio USB incompleto");Check(t.UploadStarts==1&&t.CatalogWrites==0&&!t.IsOpen,"Reintento inseguro o puerto retenido");}});
    Test("corrupcion posterior al commit impide activar catalogo",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{CorruptCommit=true};Throws(()=>f.Install(t),"no coincide al releerlo");Check(t.CatalogWrites==0,"Se activo archivo corrupto");}});
    Test("checksum sin respuesta no activa nada",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{DropInfo=true};Throws(()=>f.Install(t),"Sin respuesta a SONIDO_INFO");Check(t.UploadStarts==0&&t.CatalogWrites==0&&!t.IsOpen,"Se escribio sin verificar");Check(t.Now>=60000,"Timeout de checksum demasiado corto");}});
    Test("partido activo impide cambios",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{MatchActive=true};Throws(()=>f.Install(t),"partido activo");Check(t.UploadStarts==0&&t.InfoQueries==0,"Mutacion durante partido");}});
    Test("espacio insuficiente incluye reserva FAT",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{Free=1048576+400};Throws(()=>f.Install(t),"reserva conservadora");Check(t.UploadStarts==0,"Se escribio sin margen");}});
    Test("0.5 no admite protocolos de relectura aunque el manifiesto diga0.5",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{Firmware="0.5"};Throws(()=>f.Install(t,false,"0.5"),"firmware >=");Check(t.UploadStarts==0&&t.InfoQueries==0,"Se uso firmware incompatible");}});
    Test("version minima del manifiesto se respeta",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{Firmware="0.7"};Throws(()=>f.Install(t,false,"0.8"),"firmware >=");Check(t.UploadStarts==0,"No se respeto version minima");}});
    Test("version futura compatible y version 0.6.0 equivalen",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport{Firmware="0.6.0"};f.Install(t,false,"0.5");Check(t.CatalogWrites==1,"Comparacion de versiones incorrecta");}});
    Test("archivo local alterado falla antes de abrir transporte",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport();File.WriteAllBytes(f.Paths[0],new byte[]{1,2,3});Throws(()=>f.Install(t),"Archivo local alterado");Check(!t.Opened,"Se abrio el puerto antes de validar");}});
    Test("CRC de catalogo alterada falla antes de abrir",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport();f.Catalog[f.Catalog.Length-1]="COMENTARIOS_FIN 00000000";Throws(()=>f.Install(t),"CRC del catalogo");Check(!t.Opened,"Se abrio antes de validar catalogo");}});
    Test("mas de1024 referencias se rechaza antes de abrir",()=>{using(var f=new CommentaryFixture()){var t=new CommentaryFakeTransport();var installer=new CommentarySdInstaller(p=>t,()=>t.Now);Throws(()=>installer.Run("FALSO",f.Root,new string[1025],new string[1025],new string[1025],new string[1025],new bool[1025],new string[1027]),"Paquete de comentarios invalido");Check(!t.Opened,"Se abrio con exceso de referencias");}});
    Test("pools6a8 requieren0.7 aunque el manifiesto diga0.6",()=>{foreach(byte pool in new byte[]{6,7,8})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.6"};Throws(()=>f.Install(t,false,"0.6"),"firmware >= 0.7");Check(t.UploadStarts==0&&t.InfoQueries==0&&t.CatalogWrites==0,"Se envio pool nuevo a firmware antiguo");}});
    Test("pools6a8 se instalan y verifican con0.7",()=>{foreach(byte pool in new byte[]{6,7,8})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.7"};f.Install(t,false,"0.7");Check(t.CatalogWrites==1&&t.Files[f.Codes[0]].SequenceEqual(f.Data[0]),"Nuevo pool no compatible");}});
    Test("pools24a30 rechazan0.11a0.14 aunque manifiesto diga0.7",()=>{foreach(string fw in new[]{"0.11","0.12","0.13","0.14"})foreach(byte pool in new byte[]{24,25,26,27,28,29,30})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware=fw};Throws(()=>f.Install(t,false,"0.7"),"firmware >= 0.15");Check(t.UploadStarts==0&&t.InfoQueries==0&&t.CatalogWrites==0,"Se escribio grupo nuevo con firmware antiguo");}});
    Test("pools24a30 se instalan con0.15",()=>{foreach(byte pool in new byte[]{24,25,26,27,28,29,30})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.15"};f.Install(t,false,"0.7");Check(t.CatalogWrites==1,"No se instalo grupo 0.15");}});
    Test("pools31a38 exigen0.16",()=>{foreach(byte pool in new byte[]{31,32,33,34,35,36,37,38})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.15"};Throws(()=>f.Install(t,false,"0.7"),"firmware >= 0.16");Check(t.UploadStarts==0&&t.InfoQueries==0&&t.CatalogWrites==0,"Se escribio con version antigua");}});
    Test("pools31a38 se instalan con0.16",()=>{foreach(byte pool in new byte[]{31,32,33,34,35,36,37,38})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.16"};f.Install(t,false,"0.7");Check(t.CatalogWrites==1,"Falta catalogo");}});
    Test("audio largo exige0.16 y admite2MiB",()=>{using(var f=new CommentaryFixture()){f.Data[0]=new byte[1400000];File.WriteAllBytes(f.Paths[0],f.Data[0]);f.Crcs[0]=CommentarySdInstaller.Crc(f.Data[0]).ToString("x8");f.Hashes[0]=Convert.ToHexString(SHA256.HashData(f.Data[0])).ToLowerInvariant();var old=new CommentaryFakeTransport{Firmware="0.15"};Throws(()=>f.Install(old,false,"0.7"),"firmware >= 0.16");Check(old.UploadStarts==0,"Envio largo a firmware viejo");var t=new CommentaryFakeTransport{Firmware="0.16"};t.Files[f.Codes[0]]=f.Data[0];f.Install(t,false,"0.7");Check(t.CatalogWrites==1&&t.UploadStarts==0,"No verifico audio largo");}});
    Test("pool39 aun no registrado y se rechaza sin abrir",()=>{using(var f=new CommentaryFixture()){f.SetPool(39);var t=new CommentaryFakeTransport{Firmware="0.7"};Throws(()=>f.Install(t,false,"0.7"),"Fila de catalogo");Check(!t.Opened,"Se acepto pool fuera de limite");}});
    Test("reservados se guardan y verifican sin activar sus codigos",()=>{using(var f=new CommentaryFixture(3)){var t=new CommentaryFakeTransport{Firmware="0.7"};f.Install(t,commands:f.SubsetCatalog(0));Check(t.Files.Count==3&&t.CatalogWrites==1,"No se guardaron todos los audios");Check(t.Commands.Count(c=>c.StartsWith("COMENTARIOS_ITEM "))==1&&!t.Commands.Contains("COMENTARIOS_ITEM 0 "+f.Codes[1])&&!t.Commands.Contains("COMENTARIOS_ITEM 0 "+f.Codes[2]),"Se activaron reservados");Check(t.InfoQueries>=12,"No se verificaron los reservados");}});
    Test("reservado local corrupto falla antes de abrir",()=>{using(var f=new CommentaryFixture(2)){File.WriteAllBytes(f.Paths[1],new byte[]{1});var t=new CommentaryFakeTransport();Throws(()=>f.Install(t,commands:f.SubsetCatalog(0)),"Archivo local alterado");Check(!t.Opened,"No se valido el reservado antes de abrir");}});
    Test("catalogo no puede referir archivos ajenos al paquete",()=>{using(var f=new CommentaryFixture(2)){var commands=f.SubsetCatalog(0);commands[1]="COMENTARIOS_ITEM 0 ffffffff";var t=new CommentaryFakeTransport();Throws(()=>f.Install(t,commands:commands),"Fila de catalogo");Check(!t.Opened,"Se abrio con referencia ajena");}});
    Test("duplicado activo se rechaza incluso con reservados",()=>{using(var f=new CommentaryFixture(3)){var t=new CommentaryFakeTransport();Throws(()=>f.Install(t,commands:f.SubsetCatalog(0,0)),"Fila de catalogo");Check(!t.Opened,"Se abrio con duplicado activo");}});
    Test("corrupcion de reservado impide activar el nuevo catalogo",()=>{using(var f=new CommentaryFixture(2)){var t=new CommentaryFakeTransport{Firmware="0.7",CorruptCommit=true};t.Files[f.Codes[0]]=f.Data[0].ToArray();Throws(()=>f.Install(t,commands:f.SubsetCatalog(0)),"no coincide al releerlo");Check(t.CatalogWrites==0,"Se dio por completo un paquete con reservado corrupto");}});
    Test("pools9y10 rechazan0.9 aunque manifiesto diga0.7",()=>{foreach(byte pool in new byte[]{9,10})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.9"};Throws(()=>f.Install(t,false,"0.7"),"firmware >= 0.10");Check(t.UploadStarts==0&&t.InfoQueries==0&&t.CatalogWrites==0,"Se escribio con firmware antiguo");}});
    Test("pools9y10 se instalan con0.10",()=>{foreach(byte pool in new byte[]{9,10})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.10"};f.Install(t,false,"0.7");Check(t.CatalogWrites==1,"No se instalo grupo de dos digitos");}});
    Test("pools11a23 rechazan0.10",()=>{foreach(byte pool in new byte[]{11,12,13,14,15,16,17,18,19,20,21,22,23})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.10"};Throws(()=>f.Install(t,false,"0.7"),"firmware >= 0.11");Check(t.UploadStarts==0&&t.InfoQueries==0&&t.CatalogWrites==0,"Se envio un evento nuevo a firmware antiguo");}});
    Test("pools11a23 compatibles con0.11",()=>{foreach(byte pool in new byte[]{11,12,13,14,15,16,17,18,19,20,21,22,23})using(var f=new CommentaryFixture()){f.SetPool(pool);var t=new CommentaryFakeTransport{Firmware="0.11"};f.Install(t,false,"0.7");Check(t.CatalogWrites==1,"Grupo de eventos no instalado");}});
    Console.WriteLine("INSTALADOR_PRUEBAS_OK "+passed+" casos, sin puertos reales");
  }
}

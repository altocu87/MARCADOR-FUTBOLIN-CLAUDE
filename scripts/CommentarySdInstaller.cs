// Instalador USB de datos, separado del firmware. No reproduce voces ni inicia partidos.
using System;
using System.IO;
using System.IO.Ports;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.Security.Cryptography;
using System.Threading;
public sealed class CommentarySdInstaller {
  SerialPort serial;
  string log;
  void Record(string value){File.AppendAllText(log,DateTime.UtcNow.ToString("o")+" "+value+Environment.NewLine);}
  string Exchange(string command,string prefix,int seconds=8){
    serial.Write(command+"\n");var watch=Stopwatch.StartNew();
    while(watch.ElapsedMilliseconds<seconds*1000){
      try{
        string line=serial.ReadLine().TrimEnd('\r');
        if(line.StartsWith("SONIDO_ERROR") || line.StartsWith("SONIDOS_ERROR") || line.StartsWith("COMENTARIOS_ERROR"))throw new Exception(line);
        if(line.StartsWith(prefix))return line;
        Record("INFO "+line);
      }catch(TimeoutException){}
    }
    throw new Exception("Sin respuesta a "+command.Split(' ')[0]);
  }
  uint Crc(byte[] data){uint crc=0xffffffff;foreach(byte b in data){crc^=b;for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1)!=0?0xedb88320:0);}return crc^0xffffffff;}
  string Inventory(){
    string inventory="";
    for(int page=0;page<32;page++){
      string line=Exchange("SONIDOS_PAGINA "+page,"SONIDOS_PAGINA ");
      var count=Regex.Match(line,"total=([0-9]+)");
      if(!count.Success)throw new Exception("Respuesta de inventario incorrecta");
      int total=int.Parse(count.Groups[1].Value);
      if(total>1024)throw new Exception("Limite de inventario excedido");
      inventory+=" "+line;
      if((page+1)*32>=total)return inventory;
    }
    return inventory;
  }
  public void Run(string port,string package,string[] paths,string[] codes,string[] crcs,string[] hashes,bool[] verified,string[] catalogCommands){
    log=Path.Combine(package,"instalacion-comentarios.log");
    serial=new SerialPort(port,115200);serial.DtrEnable=false;serial.RtsEnable=false;
    serial.ReadBufferSize=65536;serial.ReadTimeout=250;serial.WriteTimeout=3000;
    try{
      serial.Open();var boot=Stopwatch.StartNew();
      while(boot.ElapsedMilliseconds<6000){try{Record(serial.ReadLine().TrimEnd('\r'));}catch(TimeoutException){}}
      string hello=Exchange("HELLO","HELLO ");Record(hello);
      if(!hello.StartsWith("HELLO waveshare-7c 0.5 "))throw new Exception("Se requiere Waveshare 7C con firmware 0.5");
      string info=Exchange("SDINFO?","SDINFO ");Record(info);
      var free=Regex.Match(info,"libre=([0-9]+)");
      if(!info.Contains("disponible=1") || !info.Contains("partido=0") || !free.Success)throw new Exception("SD no disponible o hay un partido activo");
      string inventory=Inventory();Record(inventory);
      long needed=0;bool[] skip=new bool[codes.Length];
      for(int i=0;i<codes.Length;i++){
        bool exists=inventory.Contains(codes[i]+"@s");
        bool ours=verified[i] || File.ReadAllText(log).Contains("DONE "+codes[i]+" sha256="+hashes[i]);
        if(exists && !ours)throw new Exception("Archivo existente sin verificacion propia: "+codes[i]);
        skip[i]=exists && ours;
        if(!skip[i])needed+=new FileInfo(paths[i]).Length;
        byte[] local=File.ReadAllBytes(paths[i]);
        if(local.Length>1048576 || Crc(local).ToString("x8")!=crcs[i] || Convert.ToHexString(SHA256.HashData(local)).ToLowerInvariant()!=hashes[i])throw new Exception("Archivo local alterado: "+codes[i]);
      }
      // Reserva conservadora para clusters, carpetas y catálogo; abortar antes de escribir si falta espacio.
      if(ulong.Parse(free.Groups[1].Value)<(ulong)needed+1048576)throw new Exception("Espacio insuficiente: hacen falta "+needed+" bytes mas 1 MB de margen; "+info);
      for(int i=0;i<codes.Length;i++){
        if(skip[i]){Console.WriteLine("Conservado "+codes[i]);continue;}
        byte[] data=File.ReadAllBytes(paths[i]);
        for(int attempt=1;attempt<=3;attempt++){
          try{
            string ready=Exchange("SONIDO_SUBIR sd "+codes[i]+" "+data.Length,"SONIDO_LISTO ");
            if(ready!="SONIDO_LISTO "+codes[i])throw new Exception(ready);
            for(int at=0;at<data.Length;at+=192){
              int size=Math.Min(192,data.Length-at);
              string ack=Exchange("SONIDO_DATOS "+Convert.ToBase64String(data,at,size),"SONIDO_OK ");
              if(ack!="SONIDO_OK "+(at+size))throw new Exception(ack);
            }
            string done=Exchange("SONIDO_FIN "+crcs[i],"SONIDO_GUARDADO ");
            if(done!="SONIDO_GUARDADO "+codes[i])throw new Exception(done);
            Record("DONE "+codes[i]+" sha256="+hashes[i]);Console.WriteLine((i+1)+"/"+codes.Length+" "+done);break;
          }catch(Exception error){
            if(!error.Message.EndsWith(" tiempo") || attempt==3)throw;
            Record("RETRY "+codes[i]);Thread.Sleep(200);serial.ReadExisting();
          }
        }
      }
      inventory=Inventory();foreach(string code in codes)if(!inventory.Contains(code+"@s"))throw new Exception("Falta en SD: "+code);
      foreach(string command in catalogCommands){
        string expected=command.StartsWith("COMENTARIOS_INICIO ")?"COMENTARIOS_LISTO":command.StartsWith("COMENTARIOS_ITEM ")?"COMENTARIOS_ITEM_OK":"COMENTARIOS_GUARDADO";
        string result=Exchange(command,expected,40);if(result!=expected)throw new Exception(result);
      }
      string status=Exchange("COMENTARIOS?","COMENTARIOS ");Record(status);Console.WriteLine(status);
      if(!status.Contains("total="+codes.Length+" "))throw new Exception("Cantidad activa inesperada");
      Record("CATALOGO_ACTIVO "+codes.Length);Console.WriteLine("INSTALACION_DATOS_VERIFICADA");
    }finally{if(serial.IsOpen)serial.Close();serial.Dispose();}
  }
}

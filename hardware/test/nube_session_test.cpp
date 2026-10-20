// Persistencia de cuenta sin caducidad local, opción temporal y recuperación de escritura.
// Ficheros sintéticos en build/: no toca cuentas reales ni una placa.
#include <cassert>
#include <cstring>
#include <filesystem>
#include <string>
#include "Arduino.h"
#include "mfv3_sound.h"
#include "s3_nube.h"
static const char* path = "../build/equipo-b-session-test/nube.txt";
static void fixture(const char* content) {
  FILE* f = fopen(path, "wb"); assert(f); fputs(content, f); fclose(f);
}
int main() {
  std::filesystem::create_directories("../build/equipo-b-session-test");
  snprintf(nube::stateFile, sizeof(nube::stateFile), "%s", path);
  fixture("test@example.invalid\nusuario\npermiso-antiguo\ncursor\n");
  nube::loadState();
  assert(nube::rememberSession && nube::loggedIn() && !strcmp(nube::saved.refresh, "permiso-antiguo"));
  strcpy(nube::saved.refresh, "permiso-rotado"); assert(nube::saveState());
  memset(&nube::saved, 0, sizeof(nube::saved)); nube::loadState();
  assert(nube::loggedIn() && !strcmp(nube::saved.refresh, "permiso-rotado"));
  assert(nube::setRememberSession(false));
  assert(nube::loggedIn() && !strcmp(nube::saved.refresh, "permiso-rotado")); // sigue abierta hasta apagar
  nube::loadState();
  assert(!nube::loggedIn() && !nube::rememberSession);
  strcpy(nube::saved.refresh, "otro-permiso");
  assert(nube::setRememberSession(true)); nube::loadState();
  assert(nube::loggedIn() && nube::rememberSession);
  // Interrupción entre renombrados: recuperar la copia anterior.
  std::filesystem::rename(path, std::string(path) + ".bak");
  nube::loadState();
  assert(nube::loggedIn() && !strcmp(nube::saved.refresh, "otro-permiso"));
  // Fallo de escritura: no confirmar cambio de opción ni destruir el permiso persistente.
  std::filesystem::create_directory(std::string(path) + ".tmp");
  assert(!nube::setRememberSession(false) && nube::rememberSession);
  std::filesystem::remove(std::string(path) + ".tmp");
  nube::loadState(); assert(nube::loggedIn());
  // Cerrar solo aquí: borrar la copia persistente y las copias de memoria.
  nube::phase = nube::IDLE; strcpy(nube::jobSaved.refresh, "copia"); strcpy(nube::access, "jwt");
  std::filesystem::create_directory(std::string(path) + ".tmp");
  assert(!nube::logout() && nube::loggedIn());
  std::filesystem::remove(std::string(path) + ".tmp");
  assert(nube::logout()); assert(!nube::loggedIn() && !nube::jobSaved.refresh[0] && !nube::access[0]);
  nube::loadState(); assert(!nube::loggedIn() && nube::rememberSession);
  puts("Sesion nativa: compatibilidad, rotacion, recordar, temporal, recuperacion y cierre OK");
}

"""Consulta de solo lectura. Imprime únicamente límites de sesión, nunca secretos del proyecto."""
import json
import os
import sys
import urllib.error
import urllib.request

def main():
    token = os.environ.get('SUPABASE_ACCESS_TOKEN', '')
    project = os.environ.get('PROJECT_ID', '')
    if not token or not project or not project.isalnum():
        sys.exit('Falta token o identificador del proyecto en GitHub Actions')
    request = urllib.request.Request(f'https://api.supabase.com/v1/projects/{project}/config/auth',
                                     headers={'Authorization': f'Bearer {token}'}, method='GET')
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            data = json.load(response)
    except urllib.error.HTTPError as error:
        sys.exit(f'Consulta de sesiones denegada o fallida: HTTP {error.code}; hace falta auth_config_read')
    except (OSError, ValueError):
        sys.exit('No se pudo consultar la configuración de sesiones')
    required = ('sessions_timebox', 'sessions_inactivity_timeout', 'sessions_single_per_user')
    allowed = required + ('jwt_exp', 'refresh_token_rotation_enabled')
    for key in required:
        if key not in data:
            sys.exit(f'No se pudo verificar el campo {key}; no se presupone su valor')
    safe = {k: data[k] for k in allowed if k in data}
    if any(v is not None and type(v) not in (int, bool) for v in safe.values()):
        sys.exit('Formato de configuración inesperado; no se imprime la respuesta')
    print(json.dumps(safe, indent=2))
    compatible = (data['sessions_timebox'] in (None, 0) and
                  data['sessions_inactivity_timeout'] in (None, 0) and
                  data['sessions_single_per_user'] in (None, False))
    if not compatible:
        sys.exit('Hay un límite del servidor que impide conservar web y placa indefinidamente')
    print('Compatible: sin límite de duración/inactividad y con sesiones independientes por aparato.')

if __name__ == '__main__':
    main()

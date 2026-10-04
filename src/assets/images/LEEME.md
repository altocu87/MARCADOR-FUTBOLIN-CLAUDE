# Imágenes de la aplicación

Copia aquí las imágenes generadas con los nombres EXACTOS de `docs/PROMPTS_IMAGENES.md`
(formatos admitidos: .webp, .png, .jpg o .svg). La aplicación las detecta al compilar:
no hay que tocar código. Si falta una imagen se usa el diseño de respaldo actual.

## Estado
- Integradas: `categoria-bronze`, `categoria-silver`, `categoria-gold`, `categoria-platinum`, `categoria-elite`,
  `logo`, `fondo-inicio`, `modo-rapido`, `modo-caos`, `modo-clasificatorio`, `trofeo` (transparente) y 49 de los
  54 `logro-*` (recortados de las plantillas, sin fondo magenta, 256×256 .webp).
  Las ilustraciones de modo también se usan como fondo tenue del partido; la del Clasificatorio, en la victoria.
- Tarjetas del inicio a tamaño completo, con dos estados (`-off` en reposo, `-on` al activarla con el primer toque):
  `tarjeta-rapido-off/on` (robot), `tarjeta-caos-off/on` (ardilla y caracol) y
  `tarjeta-clasificatorio-off/on` (copa), 640 px de ancho, vertical.
- Botones − y + de la configuración del partido: triángulos neón con dos estados (`-off` en reposo, `-on` al pulsar):
  `boton-sumar-off/on` (verde, hacia arriba) y `boton-restar-off/on` (rojo, hacia abajo), 600 px de ancho, .webp
  transparente y recortados justo al borde del triángulo para que encajen a ras del recuadro.
  Ahora no se usan: la configuración dibuja unas pestañas con flecha en SVG (se pueden volver a poner).
- `fondo-configuracion`: fondo de la pantalla de configuración del partido (1280×768 .webp).
- `fondo-prevision`: fondo de la Previsión del Clasificatorio (1280×720 .webp, ya desenfocado).
- `ardilla-*`: la ardilla ladrona del Partido Loco, recortada de sus hojas de personaje: `ardilla-carrera-0…5`
  (ciclo de carrera de perfil), `ardilla-cuerpo-34` y `ardilla-cuerpo-perfil` (sin cola), `ardilla-cola-1…5`,
  `ardilla-mano-*` y `ardilla-cara-*` (picara, risa, guino, ladrona, sorpresa, enfado, satisfecha, burla).
  Pendiente la hoja de 8 posturas de cuerpo entero. El caracol y el gato aún no tienen imágenes (provisionales).
- `fondo-estadisticas`: fondo de las estadísticas del partido y del detalle del historial (1280×720 .webp, ya desenfocado).
- `fondo-equipo-blanco` y `fondo-equipo-azul`: fondos de la selección de jugadores; se ve uno u otro según el
  equipo que está eligiendo (1280×720 .webp).
- Pendientes (llegaron dañadas en el ZIP; mientras, se ve el icono de respaldo):
  - `categoria-diamond` (archivo vacío).
  - `categoria-scrap` (Chatarra) y `categoria-wood` (Madera): rangos nuevos, aún sin imagen.
  - `categoria-elite` ya no se usa (el rango Élite desapareció).
  - Fila inferior de `plantilla-logros-03` (imagen cortada): `logro-hat_trick`, `logro-scorer50`,
    `logro-first_blood`, `logro-ranked_debut`, `logro-platinum`.

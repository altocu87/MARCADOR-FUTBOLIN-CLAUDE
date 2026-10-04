# Prompts de imágenes · MARCADOR FUTBOLÍN V3

Genera cada imagen con tu herramienta y guárdala en **`src/assets/images/`** con el **nombre exacto** de la
columna «Archivo» (puedes usar `.webp`, `.png`, `.jpg` o `.svg`). La aplicación las detecta sola al compilar
(`npm run dev` o `npm run build`): no hay que tocar código. Si falta alguna, se sigue viendo el diseño actual.

Recomendaciones generales:
- Formato **.webp o .png con fondo transparente** salvo los fondos.
- Paleta de la app: fondo `#070B12`, superficies `#11223B`, acento cian `#62D6FF`, blanco equipo `#EEF6FF`,
  azul equipo `#176DBC`, Caos violeta `#D65CFF`, Clasificatorio dorado `#F2C94C`.
- **Sin texto dentro de las imágenes** (los títulos los pone la app, así se leen bien y se pueden traducir).
- Que se entiendan en pequeño: siluetas claras y contraste alto.

---

## 1. Fondo de la pantalla de inicio

| Archivo | Medida | Dónde aparece |
|---|---|---|
| `fondo-inicio` | 1600 × 960 (horizontal, 5:3) | Detrás del menú principal |

**Prompt:**
> Fondo horizontal para la pantalla de inicio de un marcador digital de futbolín, estética futurista arcade y
> tecnológica. Vista en perspectiva de una mesa de futbolín de neón en un espacio oscuro azul marino casi negro
> (#070B12), rejilla luminosa tipo «tron» en el suelo que se pierde en el horizonte, líneas de luz cian (#62D6FF)
> y toques sutiles de violeta y dorado, niebla volumétrica suave, partículas de luz flotando. Composición
> oscura y poco recargada en el centro y la parte superior para que encima se lea texto blanco. Sin texto,
> sin logotipos, sin personas. Alta resolución, iluminación cinematográfica.

---

## 2. Logotipo

| Archivo | Medida | Dónde aparece |
|---|---|---|
| `logo` | 512 × 512 (cuadrado, fondo transparente) | Junto al nombre «MARCADOR FUTBOLÍN V3» arriba a la izquierda |

**Prompt:**
> Icono de aplicación / logotipo minimalista para «Marcador Futbolín», estilo futurista neón. Vista cenital
> esquemática de una mesa de futbolín dentro de un cuadrado redondeado, líneas de luz cian (#62D6FF), un balón
> blanco brillante en el centro, una portería blanca a la izquierda y una azul (#176DBC) a la derecha. Fondo
> transparente, trazos limpios y gruesos, legible a 44 píxeles, sin texto.

---

## 3. Ilustraciones de las tres modalidades

Ocupan la parte superior de cada tarjeta del inicio (la app funde el borde inferior con un degradado).

| Archivo | Medida | Tarjeta |
|---|---|---|
| `modo-rapido` | 640 × 400 | RÁPIDO |
| `modo-caos` | 640 × 400 | CAOS |
| `modo-clasificatorio` | 640 × 400 | CLASIFICATORIO |

**Prompt `modo-rapido`:**
> Ilustración horizontal estilo arcade futurista: un balón de futbolín blanco disparado a gran velocidad
> dejando una estela de luz cian (#62D6FF) y un rayo eléctrico, líneas de velocidad, fondo azul marino muy
> oscuro (#070B12) con brillo radial. Sensación de rapidez y energía. Sin texto. Parte inferior oscura y limpia.

**Prompt `modo-caos`:**
> Ilustración horizontal estilo arcade futurista: un vórtice de energía violeta y magenta (#D65CFF) con
> fragmentos tipo «glitch», chispas eléctricas y un balón de futbolín girando en el centro, distorsión digital,
> fondo casi negro. Sensación de caos divertido e impredecible. Sin texto. Parte inferior oscura y limpia.

**Prompt `modo-clasificatorio`:**
> Ilustración horizontal estilo arcade futurista: una copa de campeón dorada (#F2C94C) con reflejos metálicos
> sobre un pedestal de luz, rayos de luz dorados detrás, pequeñas partículas brillantes, fondo azul marino muy
> oscuro. Sensación de competición seria y prestigio. Sin texto. Parte inferior oscura y limpia.

---

## 4. Insignias de categoría (ELO)

Escudos que aparecen en el ranking, el perfil y la celebración de ascenso. Mismo diseño base, cambia el material.

| Archivo | Categoría | Material / color |
|---|---|---|
| `categoria-scrap` | Chatarra | chapa oxidada y remendada `#9C8F86` |
| `categoria-wood` | Madera | madera tallada con vetas `#B5814A` |
| `categoria-bronze` | Bronce | bronce cobrizo `#C98A54` |
| `categoria-silver` | Plata | plata pulida `#C9D3E0` |
| `categoria-gold` | Oro | oro brillante `#F2C94C` |
| `categoria-platinum` | Platino | platino turquesa `#7FE3D6` |
| `categoria-diamond` | Diamante | cristal azul `#8AB8FF` |

Medida: **512 × 512, fondo transparente.**

**Prompt base (cambia [MATERIAL] y [EXTRA]):**
> Insignia de rango para videojuego competitivo, escudo heráldico futurista visto de frente, hecho de
> [MATERIAL], biselado con reflejos metálicos, borde con brillo neón, un balón de futbolín estilizado en el
> centro, [EXTRA]. Fondo transparente, centrado, sin texto, estilo vectorial pulido con relieve, 512×512.

- Chatarra → MATERIAL: «chapa oxidada con remaches y remiendos» · EXTRA: «diseño muy sencillo, abollado, sin alas».
- Madera → «madera tallada con vetas visibles» · «diseño sencillo, sin alas».
- Bronce → «bronce cobrizo envejecido» · «diseño sencillo, sin alas».
- Plata → «plata pulida» · «pequeñas alas laterales».
- Oro → «oro brillante» · «alas laterales y una estrella arriba».
- Platino → «platino con reflejos turquesa» · «alas amplias y dos estrellas».
- Diamante (rango máximo) → «cristal de diamante azul tallado y translúcido» · «corona, alas de cristal y destellos».

---

## 5. Trofeo de torneo

| Archivo | Medida | Dónde aparece |
|---|---|---|
| `trofeo` | 512 × 512, fondo transparente | Lista de torneos y cartel de campeón |

**Prompt:**
> Copa de campeón de futbolín estilo futurista: copa dorada (#F2C94C) con asas, un balón de futbolín en la
> parte superior, base con anillos de luz cian, reflejos metálicos y destellos, vista frontal ligeramente
> desde abajo. Fondo transparente, sin texto, 512×512.

---

## 6. Iconos de logros (54)

Aparecen en el perfil (pestaña Logros) y en la celebración tras el partido. Medida **256 × 256 o 512 × 512,
fondo transparente**. Usa el **mismo estilo para todos** y añade el motivo de cada fila.

**Prompt base:**
> Estilo común: icono de logro de videojuego futurista dentro de un medallón redondo, colores neón con brillo cian (#62D6FF) pensados para verse sobre un fondo azul marino casi negro, líneas limpias, sin texto, centrado, fondo transparente (sin fondo), ilustración vectorial plana con ligero relieve, 512×512. Motivo: [MOTIVO].

| Archivo | Logro (rareza) | Motivo |
|---|---|---|
| `logro-debut` | Debut (común) | una bandera de salida ondeando, marco cian sencillo |
| `logro-veteran` | Veterano (rara) | una estrella con tres galones militares, marco azul eléctrico con brillo |
| `logro-legend` | Leyenda de la mesa (épica) | una corona sobre una mesa de futbolín, marco violeta/dorado con destellos intensos |
| `logro-level10` | Nivel 10 (rara) | una flecha ascendente con el número 10, marco azul eléctrico con brillo |
| `logro-level25` | Nivel 25 (épica) | doble flecha ascendente con el número 25, marco violeta/dorado con destellos intensos |
| `logro-level50` | Medio centenar (épica) | una estrella de ocho puntas con el número 50, marco violeta/dorado con destellos intensos |
| `logro-challenger` | Retador (común) | una diana con un dardo en el centro, marco cian sencillo |
| `logro-tireless` | Incansable (rara) | el símbolo infinito hecho de luz, marco azul eléctrico con brillo |
| `logro-first_win` | Primera victoria (común) | una marca de verificación dentro de un balón, marco cian sencillo |
| `logro-wins10` | Diez victorias (rara) | una medalla con el número 10, marco azul eléctrico con brillo |
| `logro-wins50` | Cincuenta victorias (épica) | una medalla grande con el número 50, marco violeta/dorado con destellos intensos |
| `logro-wins100` | Centenario (épica) | un trofeo con el número 100, marco violeta/dorado con destellos intensos |
| `logro-ranked_wins10` | Competitivo (rara) | dos espadas cruzadas sobre un escudo, marco azul eléctrico con brillo |
| `logro-express` | Exprés (rara) | un cronómetro con un rayo, marco azul eléctrico con brillo |
| `logro-dynamic_duo` | Dúo dinámico (rara) | dos muñecos de futbolín chocando las manos, marco azul eléctrico con brillo |
| `logro-streak3` | En racha (común) | tres llamas pequeñas en fila, marco cian sencillo |
| `logro-streak5` | Imparable (rara) | un rayo envuelto en fuego, marco azul eléctrico con brillo |
| `logro-streak10` | Invencible (épica) | una torre de castillo indestructible, marco violeta/dorado con destellos intensos |
| `logro-phoenix` | Fénix (rara) | un ave fénix renaciendo de las llamas, marco azul eléctrico con brillo |
| `logro-shutout` | Portería a cero (rara) | una portería con un muro de ladrillos de neón delante, marco azul eléctrico con brillo |
| `logro-rout` | Goleada (rara) | una explosión de balones, marco azul eléctrico con brillo |
| `logro-double_digits` | Doble dígito (común) | el número 10 brillante con un balón como cero, marco cian sencillo |
| `logro-goal_fest` | Festival (común) | confeti y fuegos artificiales sobre una portería, marco cian sencillo |
| `logro-early_goal` | Madrugador (rara) | un cometa con forma de balón, marco azul eléctrico con brillo |
| `logro-run5` | Apisonadora (rara) | una apisonadora aplastando balones, marco azul eléctrico con brillo |
| `logro-hat_trick` | Hat-trick (rara) | tres balones apilados con un sombrero de copa, marco azul eléctrico con brillo |
| `logro-scorer50` | Goleador (épica) | una red de portería abultada por muchos balones, marco violeta/dorado con destellos intensos |
| `logro-first_blood` | Primera sangre (común) | un balón con una gota roja de neón, marco cian sencillo |
| `logro-ranked_debut` | Competidor (común) | un casco de gladiador, marco cian sencillo |
| `logro-platinum` | Platino (rara) | un rombo de platino, marco azul eléctrico con brillo |
| `logro-diamond` | Diamante (épica) | un diamante tallado brillante, marco violeta/dorado con destellos intensos |
| `logro-elite` | Élite (épica) | una corona real morada, marco violeta/dorado con destellos intensos |
| `logro-giant_killer` | Matagigantes (rara) | un martillo derribando un gigante, marco azul eléctrico con brillo |
| `logro-champion` | Campeón (épica) | una copa de campeón dorada, marco violeta/dorado con destellos intensos |
| `logro-tri_champion` | Tricampeón (épica) | tres copas doradas juntas, marco violeta/dorado con destellos intensos |
| `logro-golden_goal` | Gol de oro (rara) | un balón dorado con un reloj, marco azul eléctrico con brillo |
| `logro-ice_cold` | Sangre fría (rara) | un balón congelado en un cubo de hielo, marco azul eléctrico con brillo |
| `logro-perfect_pens` | Penaltis perfectos (rara) | una diana con cinco balones en el centro, marco azul eléctrico con brillo |
| `logro-sudden_death` | Al límite (rara) | una calavera con un balón, marco azul eléctrico con brillo |
| `logro-eternal` | Tanda eterna (rara) | un reloj de arena que nunca se vacía, marco azul eléctrico con brillo |
| `logro-comeback` | Remontada (épica) | una flecha circular de remontada, marco violeta/dorado con destellos intensos |
| `logro-against_ropes` | Contra las cuerdas (épica) | un guante de boxeo contra las cuerdas de un ring, marco violeta/dorado con destellos intensos |
| `logro-impossible` | Misión imposible (épica) | una estrella fugaz atravesando un muro, marco violeta/dorado con destellos intensos |
| `logro-chaos_win` | Caos controlado (común) | un destello de estrella violeta, marco cian sencillo |
| `logro-chaos_lord` | Señor del Caos (rara) | un vórtice violeta con una corona, marco azul eléctrico con brillo |
| `logro-joker_win` | Comodín ganador (común) | una carta de comodín con un x2, marco cian sencillo |
| `logro-social` | Sociable (común) | un grupo de cuatro muñecos de futbolín sonrientes, marco cian sencillo |
| `logro-globetrotter` | Trotamundos (rara) | un globo terráqueo con un balón, marco azul eléctrico con brillo |
| `logro-night_owl` | Búho (común) | un búho con gafas de neón bajo la luna, marco cian sencillo |
| `logro-early_bird` | Al alba (común) | un sol saliendo sobre una mesa de futbolín, marco cian sencillo |
| `logro-marathon_day` | Día de maratón (rara) | una zapatilla de correr con estela de luz, marco azul eléctrico con brillo |
| `logro-secret_marathon` | Maratón (rara) | un reloj de arena grande con un balón, marco azul eléctrico con brillo |
| `logro-seer` | Vidente (rara) | una bola de cristal con un balón dentro, marco azul eléctrico con brillo |
| `logro-seer_debut` | Ojo clínico (común) | un ojo brillante con pupila de balón, marco cian sencillo |

Consejo: genera primero 3 o 4 para fijar el estilo y usa esas imágenes como referencia para el resto.

---

## Resumen de archivos

`fondo-inicio`, `logo`, `modo-rapido`, `modo-caos`, `modo-clasificatorio`, `trofeo`,
`categoria-scrap`, `categoria-wood`, `categoria-bronze`, `categoria-silver`, `categoria-gold`,
`categoria-platinum`, `categoria-diamond` y los `logro-*` de la tabla anterior.

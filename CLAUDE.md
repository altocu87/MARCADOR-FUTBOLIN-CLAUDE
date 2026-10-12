# Marcador Futbolín — normas de diseño

- Siempre que aparezca la ficha de un jugador (tarjetas, carteles, listas, resúmenes…),
  enseña su foto (`player.photo`, con `Avatar` o `RankPhoto`; si no tiene, sus iniciales).
- «master» significa la rama `main`.
- Pantalla táctil y decisiones rápidas: las opciones a elegir son botones grandes con solo su título
  (la elegida se ilumina). Nada de puntos de radio ni casillas que parezcan lo que hay que pulsar, ni
  descripciones debajo de cada opción. Si una opción no se entiende sola, lleva una «?» al lado que la
  explica al tocarla («Al azar» no la necesita; «Equilibradas», sí).
- Opciones que dependen de otra (p. ej. «Cómo se forman las parejas» solo con «Parejas fijas»): van en un
  panel hijo (`SubPanel`) con el nombre de su opción madre arriba («↳ Parejas fijas»), y la opción madre
  elegida se une a él con una pestaña. Botones y letras grandes siempre que sobre sitio.
